/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>
#include <optional>

#include <boost/iterator/filter_iterator.hpp>
#include <fmt/format.h>

#include <ReflectionAnalysis.h>
#include <Show.h>
#include <TypeInference.h>
#include <Walkers.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/EventLogger.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/OperatingSystem.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Timer.h>
#include <mariana-trench/Types.h>
#include <mariana-trench/type-analysis/DexTypeEnvironment.h>

namespace marianatrench {

namespace {

static const std::unordered_set<const DexType*> empty_local_extends;

static const TypeEnvironment empty_environment;

static const TypeEnvironments empty_environments;

/**
 * Check if the code block includes instructions that require a reflection
 * analysis.
 */
bool has_reflection(const IRCode& code) {
  for (cfg::Block* block : code.cfg().blocks()) {
    for (auto& entry : InstructionIterable(block)) {
      IRInstruction* instruction = entry.insn;
      if (!opcode::is_an_invoke(instruction->opcode())) {
        continue;
      }

      const auto* method = instruction->get_method();
      if (method->get_class()->str() == type::java_lang_Class()->str()) {
        return true;
      }

      if (method->get_proto()->get_rtype()->str() ==
          type::java_lang_Class()->str()) {
        return true;
      }

      auto* dex_arguments = method->get_proto()->get_args();
      if (std::any_of(
              dex_arguments->begin(), dex_arguments->end(), [](DexType* type) {
                return type->str() == type::java_lang_Class()->str();
              })) {
        return true;
      }
    }
  }

  return false;
}

bool is_interesting_opcode(IROpcode opcode) {
  return opcode::is_an_invoke(opcode) || opcode::is_an_iput(opcode);
}

/**
 * Create the environments for a method using the result from redex's reflection
 * analysis. This extracts what the analysis requires and discards the rest.
 */
TypeEnvironments make_environments(reflection::ReflectionAnalysis& analysis) {
  TypeEnvironments result;

  for (const auto& [instruction, reflection_site] :
       analysis.get_reflection_sites()) {
    TypeEnvironment environment;
    for (const auto& [register_id, entry] : reflection_site) {
      auto& abstract_object = entry.first;
      if (abstract_object.obj_kind == reflection::AbstractObjectKind::CLASS &&
          abstract_object.dex_type != nullptr) {
        environment.emplace(register_id, abstract_object.dex_type);
      }
    }
    result.emplace(instruction, std::move(environment));
  }

  return result;
}

/**
 * Create the environments for a method using the result from redex's type
 * inference. This extracts what the analysis requires and discards the rest.
 */
TypeEnvironments make_environments(
    const std::unordered_map<
        const IRInstruction*,
        type_inference::TypeEnvironment>& environments) {
  TypeEnvironments result;

  for (const auto& [instruction, types] : environments) {
    if (!is_interesting_opcode(instruction->opcode())) {
      continue;
    }

    TypeEnvironment environment;
    for (auto register_id : instruction->srcs()) {
      auto type = types.get_dex_type(register_id);
      if (type && *type != nullptr) {
        environment.emplace(register_id, TypeValue(*type));
      }
    }
    result.emplace(instruction, std::move(environment));
  }

  return result;
}

bool should_log_method(
    const Method* method,
    const std::vector<std::string>& log_method_types) {
  for (const auto& substring : log_method_types) {
    if (method->show().find(substring) != std::string::npos) {
      return true;
    }
  }
  return false;
}

std::string show_locally_inferred_types(const TypeEnvironment& environment) {
  std::string types_string = "(";
  for (const auto& [ir_register, type] : environment) {
    const auto* singleton_type = type.singleton_type();
    types_string = types_string.append(fmt::format(
        "{}: {}, ",
        std::to_string(ir_register),
        singleton_type ? singleton_type->str() : "unknown"));
  }
  types_string.append(")");
  return types_string;
}

std::string show_globally_inferred_types(
    const IRInstruction* instruction,
    const RegTypeEnvironment& environment) {
  std::string types_string = "(";
  for (const auto& ir_register : instruction->srcs()) {
    types_string.append(fmt::format(
        "\n  Reg {}: {}, ",
        std::to_string(ir_register),
        show(environment.get(ir_register).get_single_domain())));

    if (const auto& result = environment.get(ir_register).get_set_domain();
        result.kind() == sparta::AbstractValueKind::Value) {
      types_string.append("Local extends: ");
      for (const auto& dex_type : result.get_types()) {
        types_string.append(fmt::format("\n     : {}, ", show(dex_type)));
      }
    }
  }
  types_string.append(")");
  return types_string;
}

std::string show_smallset_dex_types_selection(
    const SmallSetDexTypeDomain& small_set_dex_domain,
    const std::unordered_set<const DexType*>& included_types) {
  std::string small_set_types = "";

  if (small_set_dex_domain.kind() != sparta::AbstractValueKind::Value) {
    return small_set_types;
  }

  for (const auto* type : small_set_dex_domain.get_types()) {
    small_set_types.append(fmt::format(
        "\n  {} {}",
        show(type),
        included_types.find(type) == included_types.end() ? "skipped!"
                                                          : "added!"));
  }
  return small_set_types;
}

/**
 * Select the more precise/narrower type between locally_inferred_type and
 * globally_inferred_type.
 * Returns nullptr if the two types are incompatible.
 */
const DexType* MT_NULLABLE select_precise_singleton_type(
    const DexType* locally_inferred_type,
    const DexType* MT_NULLABLE globally_inferred_type) {
  if (locally_inferred_type == globally_inferred_type ||
      globally_inferred_type == nullptr) {
    return locally_inferred_type;
  }

  if (type::check_cast(
          /* type */ locally_inferred_type,
          /* base_type */ globally_inferred_type)) {
    // Local type analysis inferred a narrower type.
    return locally_inferred_type;
  } else if (!type::check_cast(
                 /* type */ globally_inferred_type,
                 /* base_type */ locally_inferred_type)) {
    // Failed to cast local to global or global to local types.
    // Two types are incompatible with each other.
    return nullptr;
  }

  // Global type analysis inferred a narrower type.
  return globally_inferred_type;
}

/**
 * Filters `small_set_dex_domain` to only types that are derived from
 * `singleton_type`.
 */
std::unordered_set<const DexType*> filter_valid_derived_types(
    const DexType* singleton_type,
    const SmallSetDexTypeDomain& small_set_dex_domain) {
  std::unordered_set<const DexType*> result{};

  if (small_set_dex_domain.kind() != sparta::AbstractValueKind::Value) {
    return result;
  }

  const auto& types = small_set_dex_domain.get_types();
  if (types.size() == 0) {
    // SmallSetDexTypeDomain can be empty for a Value kind when
    // initializing a DexTypeDomain with Nullness::IS_NULL.
    // See DexTypeDomain::null().
    return result;
  }

  auto is_valid_type = [singleton_type](const DexType* type) {
    // Global type analysis ends up storing sibling types in the
    // SmallSetDexTypeDomain in some cases, usually when generic interfaces are
    // involved. We only consider the 'type' in SmallSetDexTypeDomain as valid
    // if it is derived from singleton_type.
    // Since we select the more precise of the locally_inferred_type and the
    // globally_inferred_type as the singleton_type, this may filter out
    // types tracked as valid by the global type analysis.
    return type::check_cast(type, /* base_type */ singleton_type);
  };

  result.insert(
      boost::make_filter_iterator(is_valid_type, types.begin()),
      boost::make_filter_iterator(is_valid_type, types.end()));

  return result;
}

} // namespace

TypeValue::TypeValue(const DexType* singleton_type)
    : TypeValue(singleton_type, /* local extends */ {}) {}

TypeValue::TypeValue(
    const DexType* singleton_type,
    std::unordered_set<const DexType*> local_extends)
    : singleton_type_(singleton_type),
      local_extends_(std::move(local_extends)) {
  mt_assert(singleton_type != nullptr);
}

std::ostream& operator<<(std::ostream& out, const TypeValue& value) {
  out << "TypeValue(";
  if (value.singleton_type_ != nullptr) {
    out << "singleton_type=`" << show(value.singleton_type_) << "`,";
  }
  if (!value.local_extends_.empty()) {
    out << "local_extends={";
    for (const auto* type : value.local_extends_) {
      out << show(type) << ", ";
    }
    out << "}";
  }

  return out << ")";
}

Types::Types(const Options& options, const DexStoresVector& stores) {
  Scope scope = build_class_scope(stores);
  log_method_types_ = options.log_method_types();
  scope.erase(
      std::remove_if(
          scope.begin(),
          scope.end(),
          [](DexClass* dex_class) { return dex_class->is_external(); }),
      scope.end());

  if (!options.disable_global_type_analysis()) {
    Timer global_timer;
    auto analysis = marianatrench::type_analyzer::global::GlobalTypeAnalysis::
        make_default();
    global_type_analyzer_ = analysis.analyze(scope, options);

    LOG(1,
        "Global analysis {:.2f}s. Memory used, RSS: {:.2f}GB",
        global_timer.duration_in_seconds(),
        resident_set_size_in_gb());
  } else {
    LOG(1, "Disabled global type analysis.");
    global_type_analyzer_ = nullptr;
  }

  // ReflectionAnalysis must run after GlobalTypeAnalysis. ReflectionAnalysis
  // causes the cfg to be non-editable (code.cfg().editable() == false) but
  // GlobalTypeAnalysis requires it to be editable. Alternatively, one can
  // also re-build the cfg with editable set to true prior to running
  // GlobalTypeAnalysis.
  Timer reflection_timer;
  reflection::MetadataCache reflection_metadata_cache;
  walk::parallel::code(
      scope,
      [this, &reflection_metadata_cache](DexMethod* method, IRCode& code) {
        mt_assert(code.cfg_built());

        if (!has_reflection(code)) {
          return;
        }

        reflection::ReflectionAnalysis analysis(
            /* dex_method */ method,
            /* context */ nullptr,
            /* summary_query_fn */ nullptr,
            /* metadata_cache */ &reflection_metadata_cache);

        const_class_environments_.emplace(
            method,
            std::make_unique<TypeEnvironments>(make_environments(analysis)));
      });
  LOG(1,
      "Reflection analysis {:.2f}s. Memory used, RSS: {:.2f}GB",
      reflection_timer.duration_in_seconds(),
      resident_set_size_in_gb());
}

std::unique_ptr<TypeEnvironments> Types::infer_local_types_for_method(
    const Method* method) const {
  auto* code = method->get_code();
  if (!code) {
    WARNING(
        4,
        "Trying to get local types for `{}` which does not have code.",
        method->show());
    return std::make_unique<TypeEnvironments>();
  }

  auto* parameter_type_list = method->get_proto()->get_args();
  const auto& parameter_type_overrides = method->parameter_type_overrides();
  if (!parameter_type_overrides.empty()) {
    DexTypeList::ContainerType new_parameter_type_list;
    for (std::size_t parameter_position = 0;
         parameter_position < parameter_type_list->size();
         parameter_position++) {
      auto override = parameter_type_overrides.find(parameter_position);
      // Override parameter type if it's present in the map.
      new_parameter_type_list.push_back(const_cast<DexType*>(
          override == parameter_type_overrides.end()
              ? parameter_type_list->at(parameter_position)
              : override->second));
    }

    parameter_type_list =
        DexTypeList::make_type_list(std::move(new_parameter_type_list));
  }

  try {
    type_inference::TypeInference inference(
        code->cfg(),
        /* skip_check_cast_upcasting */ true);
    inference.run(
        method->is_static(), method->get_class(), parameter_type_list);
    return std::make_unique<TypeEnvironments>(
        make_environments(inference.get_type_environments()));
  } catch (const RedexException& rethrown_exception) {
    auto error = fmt::format(
        "Cannot infer types for method `{}`: {}.",
        method->show(),
        rethrown_exception.what());
    ERROR(1, error);
    EventLogger::log_event("type_inference", error, /* verbosity_level */ 1);

    return std::make_unique<TypeEnvironments>();
  }
}

std::unique_ptr<TypeEnvironments> Types::infer_types_for_method(
    const Method* method) const {
  auto log_method = should_log_method(method, log_method_types_);
  auto* code = method->get_code();
  if (!code) {
    WARNING(
        log_method ? 0 : 4,
        "Trying to get types for `{}` which does not have code.",
        method->show());
    return std::make_unique<TypeEnvironments>();
  }
  LOG(log_method ? 0 : 5,
      "Inferring types for {}\nCode:\n{}",
      method->show(),
      Method::show_control_flow_graph(code->cfg()));

  // Call TypeInference first, then use GlobalTypeAnalyzer to refine results.
  auto environments = infer_local_types_for_method(method);
  if (global_type_analyzer_ == nullptr) {
    return environments;
  }

  auto per_method_global_type_analyzer =
      global_type_analyzer_->get_replayable_local_analysis(
          method->dex_method());

  for (auto* block : code->cfg().blocks()) {
    auto current_state =
        per_method_global_type_analyzer->get_entry_state_at(block);
    for (auto& entry : InstructionIterable(block)) {
      const auto* instruction = entry.insn;
      per_method_global_type_analyzer->analyze_instruction(
          instruction, &current_state);

      LOG(log_method ? 0 : 5,
          "GTA: Analyzed instruction `{}`. RegEnv: {}",
          show(instruction),
          show(current_state.get_reg_environment()));

      if (!is_interesting_opcode(instruction->opcode())) {
        continue;
      }
      auto found = environments->find(instruction);
      auto& environment_at_instruction = found->second;

      const auto& global_type_environment = current_state.get_reg_environment();
      if (!global_type_environment.is_value()) {
        continue;
      }

      LOG(log_method ? 0 : 5,
          "Caller: {} Instruction: {}\nLocally Inferred types: {}\nGlobally Inferred types: {}",
          method->show(),
          show(instruction),
          show_locally_inferred_types(environment_at_instruction),
          show_globally_inferred_types(instruction, global_type_environment));

      for (auto ir_register : instruction->srcs()) {
        const auto& globally_inferred_type_domain =
            global_type_environment.get(ir_register);

        // DexTypeDomain is a ReducedProductAbstractDomain. i.e, if anyone of
        // the abstract domains have a _|_ component, the DexTypeDomain is set
        // to _|_.
        if (globally_inferred_type_domain.is_bottom()) {
          continue;
        }

        // Find local type to refine.
        auto result = environment_at_instruction.find(ir_register);
        if (result == environment_at_instruction.end()) {
          continue;
        }

        TypeValue* result_type_value = &result->second;
        const DexType* locally_inferred_type =
            result_type_value->singleton_type();

        const DexType* globally_inferred_type = nullptr;
        if (auto dex_type = globally_inferred_type_domain.get_dex_type()) {
          globally_inferred_type = *dex_type;
        }

        // Select the more precise of the two available types.
        const auto* precise_singleton_type = select_precise_singleton_type(
            locally_inferred_type, globally_inferred_type);

        if (precise_singleton_type == nullptr) {
          // Two available types are incompatible. Keep the local type.
          LOG(log_method ? 0 : 5,
              "Global type analysis inferred incompatible type `{}` compared to local type analysis `{}` for register {} in instruction {} of method {}.",
              show(locally_inferred_type),
              show(globally_inferred_type),
              std::to_string(ir_register),
              show(instruction),
              method->show());
          continue;
        }

        // Refine the TypeValue if possible.
        result_type_value->set_singleton_type(precise_singleton_type);
        auto valid_derived_types = filter_valid_derived_types(
            precise_singleton_type,
            globally_inferred_type_domain.get_set_domain());
        result_type_value->set_local_extends(std::move(valid_derived_types));

        LOG(log_method ? 0 : 5,
            "Refined types in: Caller: {} \nInstruction: {}\nReg {}\n  Singleton Type : {}\n  Local extends: {}",
            method->show(),
            show(instruction),
            std::to_string(ir_register),
            show(result_type_value->singleton_type()),
            show_smallset_dex_types_selection(
                globally_inferred_type_domain.get_set_domain(),
                result_type_value->local_extends()));
      }
    }
  }

  return environments;
}

const TypeEnvironments& Types::environments(const Method* method) const {
  auto* environments = environments_.get(method, /* default */ nullptr);
  if (environments != nullptr) {
    return *environments;
  }

  auto* code = method->get_code();

  if (!code) {
    WARNING(
        4,
        "Trying to get types for `{}` which does not have code.",
        method->show());
    return empty_environments;
  }

  environments_.emplace(method, this->infer_types_for_method(method));
  return *environments_.at(method);
}

const TypeEnvironments& Types::const_class_environments(
    const Method* method) const {
  auto* environments = const_class_environments_.get(
      method->dex_method(), /* default */ nullptr);
  if (environments != nullptr) {
    return *environments;
  }

  return empty_environments;
}

const TypeEnvironment& Types::environment(
    const Method* method,
    const IRInstruction* instruction) const {
  const auto& environments = this->environments(method);
  auto environment = environments.find(instruction);
  if (environment == environments.end()) {
    return empty_environment;
  } else {
    return environment->second;
  }
}

const TypeEnvironment& Types::const_class_environment(
    const Method* method,
    const IRInstruction* instruction) const {
  const auto& environments = this->const_class_environments(method);
  auto environment = environments.find(instruction);
  if (environment == environments.end()) {
    return empty_environment;
  } else {
    return environment->second;
  }
}

const DexType* MT_NULLABLE Types::register_type(
    const Method* method,
    const IRInstruction* instruction,
    Register register_id) const {
  const auto& environment = this->environment(method, instruction);
  auto type = environment.find(register_id);
  if (type == environment.end()) {
    return nullptr;
  }

  return type->second.singleton_type();
}

const std::unordered_set<const DexType*>& Types::register_local_extends(
    const Method* method,
    const IRInstruction* instruction,
    Register register_id) const {
  const auto& environment = this->environment(method, instruction);
  auto type = environment.find(register_id);
  if (type == environment.end()) {
    return empty_local_extends;
  }

  return type->second.local_extends();
}

const DexType* MT_NULLABLE Types::source_type(
    const Method* method,
    const IRInstruction* instruction,
    std::size_t source_position) const {
  return register_type(method, instruction, instruction->src(source_position));
}

const DexType* MT_NULLABLE Types::receiver_type(
    const Method* method,
    const IRInstruction* instruction) const {
  mt_assert(opcode::is_an_invoke(instruction->opcode()));

  if (opcode::is_invoke_static(instruction->opcode())) {
    return nullptr;
  }

  return source_type(method, instruction, /* source_position */ 0);
}

const std::unordered_set<const DexType*>& Types::receiver_local_extends(
    const Method* method,
    const IRInstruction* instruction) const {
  mt_assert(opcode::is_an_invoke(instruction->opcode()));

  if (opcode::is_invoke_static(instruction->opcode())) {
    return empty_local_extends;
  }

  return register_local_extends(
      method, instruction, instruction->src(/* source_position */ 0));
}

const DexType* MT_NULLABLE Types::register_const_class_type(
    const Method* method,
    const IRInstruction* instruction,
    Register register_id) const {
  const auto& environment = this->const_class_environment(method, instruction);
  auto type = environment.find(register_id);
  if (type == environment.end()) {
    return nullptr;
  }

  return type->second.singleton_type();
}

} // namespace marianatrench

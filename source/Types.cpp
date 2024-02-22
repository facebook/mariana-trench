/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>
#include <optional>

#include <fmt/format.h>

#include <ProguardConfiguration.h>
#include <ProguardMap.h>
#include <ProguardMatcher.h>
#include <ProguardParser.h>
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

namespace marianatrench {

namespace {

static const TypeValue empty_type_value;

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
      if (abstract_object.obj_kind == reflection::AbstractObjectKind::CLASS) {
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

/**
 * Select the more precise type between local_type and globally_inferred_type
 * to as a singleton type representation at the ir_register for instruction in
 * caller.
 */
const DexType* MT_NULLABLE select_precise_singleton_type(
    const DexType* MT_NULLABLE local_type,
    const DexType* MT_NULLABLE globally_inferred_type,
    const Method* caller,
    const IRInstruction* instruction,
    Register ir_register,
    bool log_method) {
  if (local_type == globally_inferred_type) {
    if (local_type == nullptr) {
      WARNING(
          2,
          "Both Local type analysis and global type analysis could not determine the type for register {} in instruction {} of method {}",
          std::to_string(ir_register),
          show(instruction),
          caller->show());
    }

    return local_type;
  }

  if (local_type == nullptr) {
    return globally_inferred_type;
  } else if (globally_inferred_type == nullptr) {
    return local_type;
  }

  mt_assert(local_type != nullptr && globally_inferred_type != nullptr);

  if (type::check_cast(
          /* type */ local_type, /* base_type */ globally_inferred_type)) {
    LOG(log_method ? 0 : 5,
        "Local type analysis inferred a narrower type {} than global type analysis {} for register {} in instruction {} of method {}",
        local_type->str(),
        globally_inferred_type->str(),
        std::to_string(ir_register),
        show(instruction),
        caller->show());

    return local_type;
  } else if (!type::check_cast(
                 /* type */ globally_inferred_type,
                 /* base_type */ local_type)) {
    LOG(log_method ? 0 : 5,
        "Local type analysis inferred unrelated type `{}` from global type analysis `{}` for register {} in instruction {} of method {}.",
        local_type->str(),
        globally_inferred_type->str(),
        std::to_string(ir_register),
        show(instruction),
        caller->show());

    return local_type;
  }

  LOG(log_method ? 0 : 5,
      "Global type analysis inferred a narrower type {} than local type analysis {} for register {} in instruction {} of method {}",
      local_type->str(),
      globally_inferred_type->str(),
      std::to_string(ir_register),
      show(instruction),
      caller->show());

  return globally_inferred_type;
}

} // namespace

TypeValue::TypeValue(const DexType* dex_type) : singleton_type_(dex_type) {}

TypeValue::TypeValue(
    const DexType* singleton_type,
    const SmallSetDexTypeDomain& small_set_dex_types)
    : singleton_type_(singleton_type) {
  mt_assert(small_set_dex_types.kind() == sparta::AbstractValueKind::Value);

  const auto& types = small_set_dex_types.get_types();
  if (types.size() == 0) {
    // SmallSetDexTypeDomain can be empty for a Value kind when creating a
    // initializing a DexTypeDomain with Nullness::IS_NULL.
    // See DexTypeDomain::null().
    WARNING(
        2,
        "Empty SmallSetDexTypeDomain for singleton_type: {}",
        singleton_type->str());
    return;
  }

  local_extends_ =
      std::unordered_set<const DexType*>(types.begin(), types.end());
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
    type_analyzer::global::GlobalTypeAnalysis analysis(
        /* max_global_analysis_iteration */ 10,
        /* use_multiple_callee_callgraph */ true,
        /* only_aggregate_safely_inferrable_fields */ false);
    global_type_analyzer_ = analysis.analyze(scope);

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
    EventLogger::log_event("type_inference", error);

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
      auto* instruction = entry.insn;
      per_method_global_type_analyzer->analyze_instruction(
          instruction, &current_state);

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

      for (auto& ir_register : instruction->srcs()) {
        const auto& globally_inferred_type_domain =
            global_type_environment.get(ir_register);

        // DexTypeDomain is a ReducedProductAbstractDomain. i.e, if anyone of
        // the abstract domains have a _|_ component, the DexTypeDomain is set
        // to _|_.
        if (globally_inferred_type_domain.is_bottom()) {
          continue;
        }

        const DexType* globally_inferred_type = nullptr;
        if (auto result = globally_inferred_type_domain.get_dex_type()) {
          globally_inferred_type = *result;
        }

        const DexType* local_type = nullptr;
        if (auto result = environment_at_instruction.find(ir_register);
            result != environment_at_instruction.end()) {
          local_type = result->second.singleton_type();
        }

        const auto* singleton_type = select_precise_singleton_type(
            local_type,
            globally_inferred_type,
            method,
            instruction,
            ir_register,
            log_method);

        if (singleton_type == nullptr) {
          continue;
        }

        std::optional<TypeValue> type_value = std::nullopt;
        const auto& small_set_domain =
            globally_inferred_type_domain.get_set_domain();
        if (small_set_domain.kind() == sparta::AbstractValueKind::Value) {
          type_value = TypeValue(singleton_type, small_set_domain);
        } else if (singleton_type != local_type) {
          type_value = TypeValue(singleton_type);
        }

        if (type_value) {
          LOG(log_method ? 0 : 5,
              "Replacing locally inferred type {} with globally inferred: {} for register {} in instruction {} of method {}",
              local_type ? local_type->str() : "unknown",
              *type_value,
              std::to_string(ir_register),
              show(instruction),
              method->show());
          environment_at_instruction[ir_register] = *type_value;
        }
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
    return empty_type_value.local_extends();
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

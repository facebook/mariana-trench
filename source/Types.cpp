/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>

#include <Show.h>
#include <Walkers.h>

#include <ProguardConfiguration.h>
#include <ProguardMap.h>
#include <ProguardMatcher.h>
#include <ProguardParser.h>
#include <ReflectionAnalysis.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Types.h>

namespace marianatrench {

namespace {

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
              dex_arguments->begin(), dex_arguments->end(), [&](DexType* type) {
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
        environment.emplace(register_id, *type);
      }
    }
    result.emplace(instruction, std::move(environment));
  }

  return result;
}

} // namespace

Types::Types(const Options& options, const DexStoresVector& stores) {
  Scope scope = build_class_scope(stores);
  scope.erase(
      std::remove_if(
          scope.begin(),
          scope.end(),
          [](DexClass* dex_class) { return dex_class->is_external(); }),
      scope.end());
  reflection::MetadataCache reflection_metadata_cache;
  walk::parallel::code(scope, [&](DexMethod* method, IRCode& code) {
    if (!code.cfg_built()) {
      code.build_cfg();
    }

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
  type_analyzer::global::GlobalTypeAnalysis analysis;
  global_type_analyzer_ = analysis.analyze(scope);
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
    type_inference::TypeInference inference(code->cfg());
    inference.run(
        method->is_static(), method->get_class(), parameter_type_list);
    return std::make_unique<TypeEnvironments>(
        make_environments(inference.get_type_environments()));
  } catch (const RedexException& rethrown_exception) {
    ERROR(
        1,
        "Cannot infer types for method `{}`: {}.",
        method->show(),
        rethrown_exception.what());
    return std::make_unique<TypeEnvironments>();
  }
}

std::unique_ptr<TypeEnvironments> Types::infer_types_for_method(
    const Method* method) const {
  auto* code = method->get_code();
  if (!code) {
    WARNING(
        4,
        "Trying to get types for `{}` which does not have code.",
        method->show());
    return std::make_unique<TypeEnvironments>();
  }

  // Call TypeInference first, then use GlobalTypeAnalyzer to refine results.
  auto environments = infer_local_types_for_method(method);
  if (global_type_analyzer_ == nullptr) {
    return environments;
  }
  auto local_type_analyzer =
      global_type_analyzer_->get_local_analysis(method->dex_method());

  for (cfg::Block* block : code->cfg().blocks()) {
    auto current_state = local_type_analyzer->get_entry_state_at(block);
    for (auto& entry : InstructionIterable(block)) {
      auto* instruction = entry.insn;
      local_type_analyzer->analyze_instruction(instruction, &current_state);

      if (!is_interesting_opcode(instruction->opcode())) {
        continue;
      }
      auto found = environments->find(instruction);
      auto& environment_at_instruction = found->second;

      auto register_type_environment = current_state.get_reg_environment();
      if (!register_type_environment.is_value()) {
        continue;
      }
      for (auto& ir_register : instruction->srcs()) {
        DexTypeDomain domain = register_type_environment.get(ir_register);
        auto found = environment_at_instruction.find(ir_register);
        if (found != environment_at_instruction.end()) {
          auto dex_type = found->second;
          auto new_dex_type_domain =
              domain.is_top() ? DexTypeDomain(dex_type) : domain;
          auto new_dex_type = new_dex_type_domain.get_dex_type();
          environment_at_instruction[ir_register] =
              new_dex_type ? *new_dex_type : dex_type;
        } else {
          auto new_dex_type = domain.get_dex_type();
          if (new_dex_type) {
            environment_at_instruction[ir_register] = *new_dex_type;
          }
        }
      }

      local_type_analyzer->analyze_instruction(entry.insn, &current_state);
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

  return type->second;
}

const DexType* MT_NULLABLE Types::source_type(
    const Method* method,
    const IRInstruction* instruction,
    std::size_t source_position) const {
  return register_type(method, instruction, instruction->src(source_position));
}

const DexType* Types::receiver_type(
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

  return type->second;
}

} // namespace marianatrench

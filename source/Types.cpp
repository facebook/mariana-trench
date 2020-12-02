// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <DexInstruction.h>
#include <Show.h>
#include <Walkers.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Types.h>

namespace marianatrench {

Types::Types(const DexStoresVector& stores) {
  for (auto& scope : DexStoreClassesIterator(stores)) {
    walk::parallel::methods(scope, [&](DexMethod* method) {
      auto code = method->get_code();
      if (!code) {
        return;
      }

      if (!code->cfg_built()) {
        code->build_cfg();
        code->cfg().calculate_exit_block();
      }
    });
  }
}

namespace {

static const TypeEnvironment empty_environment;

static const TypeEnvironments empty_environments;

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
    if (!opcode::is_an_invoke(instruction->opcode()) &&
        !opcode::is_an_iput(instruction->opcode())) {
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
        show(method));
    return empty_environments;
  }

  auto* parameter_type_list = method->get_proto()->get_args();
  const auto& parameter_type_overrides = method->parameter_type_overrides();
  if (!parameter_type_overrides.empty()) {
    std::deque<DexType*> new_parameter_type_list;
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
    environments_.emplace(
        method,
        std::make_unique<TypeEnvironments>(
            make_environments(inference.get_type_environments())));
  } catch (const RedexException& rethrown_exception) {
    ERROR(
        1,
        "Cannot infer types for method `{}`: {}.",
        show(method),
        rethrown_exception.what());
    environments_.emplace(method, std::make_unique<TypeEnvironments>());
  }

  return *environments_.at(method);
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

const DexType* MT_NULLABLE Types::register_type(
    const Method* method,
    const IRInstruction* instruction,
    Register register_id) const {
  const auto& environment = this->environment(method, instruction);
  auto type = environment.find(register_id);
  if (type == environment.end()) {
    return nullptr;
  } else {
    return type->second;
  }
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

} // namespace marianatrench

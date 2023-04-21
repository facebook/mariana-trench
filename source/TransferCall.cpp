/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/TransferCall.h>

namespace marianatrench {

void log_instruction(
    const MethodContext* context,
    const IRInstruction* instruction) {
  LOG_OR_DUMP(context, 4, "Instruction: \033[33m{}\033[0m", show(instruction));
}

std::vector<const DexType * MT_NULLABLE> get_source_register_types(
    const MethodContext* context,
    const IRInstruction* instruction) {
  std::vector<const DexType* MT_NULLABLE> register_types = {};
  for (auto source_register : instruction->srcs()) {
    register_types.push_back(context->types.register_type(
        context->method(), instruction, source_register));
  }
  return register_types;
}

namespace {

std::optional<std::string> register_constant_argument(
    const RegisterMemoryLocationsMap& register_memory_locations_map,
    Register register_id) {
  const auto& memory_locations = register_memory_locations_map.at(register_id);

  auto* memory_location_singleton = memory_locations.singleton();
  if (memory_location_singleton == nullptr) {
    return std::nullopt;
  }

  MemoryLocation* memory_location = *memory_location_singleton;
  auto* instruction_memory_location =
      memory_location->as<InstructionMemoryLocation>();
  if (instruction_memory_location == nullptr) {
    return std::nullopt;
  }

  return instruction_memory_location->get_constant();
}

} // namespace

std::vector<std::optional<std::string>> get_source_constant_arguments(
    const RegisterMemoryLocationsMap& register_memory_locations_map,
    const IRInstruction* instruction) {
  std::vector<std::optional<std::string>> constant_arguments = {};
  constant_arguments.reserve(instruction->srcs_size());

  for (auto register_id : instruction->srcs()) {
    constant_arguments.push_back(
        register_constant_argument(register_memory_locations_map, register_id));
  }

  return constant_arguments;
}

CalleeModel get_callee(
    const MethodContext* context,
    const IRInstruction* instruction,
    const DexPosition* MT_NULLABLE dex_position,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments) {
  mt_assert(opcode::is_an_invoke(instruction->opcode()));

  auto call_target = context->call_graph.callee(context->method(), instruction);
  if (!call_target.resolved()) {
    WARNING_OR_DUMP(
        context,
        3,
        "Unable to resolve call to `{}`",
        show(instruction->get_method()));
  } else {
    LOG_OR_DUMP(
        context,
        4,
        "Call resolved to `{}`",
        show(call_target.resolved_base_callee()));
  }

  auto* position = context->positions.get(context->method(), dex_position);

  auto model = context->model_at_callsite(
      call_target, position, source_register_types, source_constant_arguments);
  LOG_OR_DUMP(context, 4, "Callee model: {}", model);

  // Avoid copies using `std::move`.
  // https://fb.workplace.com/groups/2292641227666517/permalink/2478196942444277/
  return CalleeModel{
      instruction->get_method(),
      call_target.resolved_base_callee(),
      position,
      call_target.call_index(),
      std::move(model)};
}

CalleeModel get_callee(
    const MethodContext* context,
    const ArtificialCallee& callee,
    const DexPosition* MT_NULLABLE dex_position) {
  const auto* resolved_base_callee = callee.call_target.resolved_base_callee();
  mt_assert(resolved_base_callee != nullptr);

  LOG_OR_DUMP(
      context, 4, "Artificial call to `{}`", show(resolved_base_callee));

  auto* position = context->positions.get(context->method(), dex_position);

  auto model = context->model_at_callsite(
      callee.call_target,
      position,
      /* source_register_types */ {},
      /* source_constant_arguments */ {});
  LOG_OR_DUMP(context, 4, "Callee model: {}", model);

  return CalleeModel{
      resolved_base_callee->dex_method(),
      resolved_base_callee,
      position,
      callee.call_target.call_index(),
      std::move(model)};
}

MemoryLocation* MT_NULLABLE try_inline_invoke(
    const MethodContext* context,
    const RegisterMemoryLocationsMap& register_memory_locations_map,
    const IRInstruction* instruction,
    const CalleeModel& callee) {
  auto access_path = callee.model.inline_as().get_constant();
  if (!access_path) {
    return nullptr;
  }

  auto register_id = instruction->src(access_path->root().parameter_position());
  auto memory_locations = register_memory_locations_map.at(register_id);

  auto* memory_location_singleton = memory_locations.singleton();
  if (memory_location_singleton == nullptr) {
    LOG_OR_DUMP(
        context,
        4,
        "Could not inline call because register {} points to multiple memory locations",
        register_id);
    return nullptr;
  }

  MemoryLocation* memory_location = *memory_location_singleton;
  for (const auto& field : access_path->path()) {
    mt_assert(field.is_field());
    memory_location = memory_location->make_field(field.name());
  }

  // Only inline if the model does not generate or propagate extra taint.
  if (!callee.model.generations().is_bottom()) {
    LOG_OR_DUMP(
        context,
        4,
        "Could not inline call because callee model has generations");
    return nullptr;
  }
  if (!callee.model.propagations().leq(TaintAccessPathTree(
          {{/* input */ *access_path,
            Taint::propagation(PropagationConfig(
                /* input_path */ *access_path,
                /* kind */ context->kind_factory.local_return(),
                /* output_paths */
                PathTreeDomain{{Path{}, CollapseDepth::zero()}},
                /* inferred_features */ {},
                /* locally_inferred_features */ {},
                /* user_features */ FeatureSet::bottom()))}}))) {
    LOG_OR_DUMP(
        context,
        4,
        "Could not inline call because callee model has extra propagations");
    return nullptr;
  }
  if (callee.model.add_via_obscure_feature() ||
      callee.model.has_add_features_to_arguments()) {
    LOG_OR_DUMP(
        context,
        4,
        "Could not inline call because callee model has add-via-obscure or add-features-to-arguments");
    return nullptr;
  }

  return memory_location;
}

namespace {

bool is_inner_class_this(const FieldMemoryLocation* location) {
  return location->parent()->is<ThisParameterMemoryLocation>() &&
      location->field()->str() == "this$0";
}

} // namespace

void add_field_features(
    MethodContext* context,
    TaintTree& taint_tree,
    const FieldMemoryLocation* field_memory_location) {
  if (!is_inner_class_this(field_memory_location)) {
    return;
  }
  auto features = FeatureMayAlwaysSet::make_always(
      {context->feature_factory.get("via-inner-class-this")});
  taint_tree.add_locally_inferred_features(features);
}

} // namespace marianatrench

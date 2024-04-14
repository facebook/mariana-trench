/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>
#include <TypeUtil.h>

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
  std::vector<const DexType * MT_NULLABLE> register_types = {};
  for (auto source_register : instruction->srcs()) {
    const auto* register_type = context->types.register_type(
        context->method(), instruction, source_register);

    if (register_type == type::java_lang_Class()) {
      if (const auto* const_class_type =
              context->types.register_const_class_type(
                  context->method(), instruction, source_register)) {
        register_type = const_class_type;
      }
    }

    register_types.push_back(register_type);
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

CallClassIntervalContext get_type_context(
    const MethodContext* context,
    const IRInstruction* instruction,
    bool is_this_call) {
  if (instruction->opcode() != OPCODE_INVOKE_VIRTUAL) {
    // Class intervals only apply to virtual calls.
    return CallClassIntervalContext();
  }

  // Virtual calls have at least one argument (the receiver).
  mt_assert(instruction->srcs_size() > 0);

  auto receiver_register = instruction->src(0);
  const auto* receiver_type = context->types.register_type(
      context->method(), instruction, receiver_register);
  if (receiver_type == nullptr) {
    WARNING(
        2,
        "Could not get type for receiver in instruction `{}`.",
        show(instruction));
    // Receiver type unknown, use top to cover all possible types.
    return CallClassIntervalContext(
        ClassIntervals::Interval::top(),
        /* preserves_type_context */ is_this_call);
  }

  auto interval = context->class_intervals.get_interval(receiver_type);

  LOG_OR_DUMP(
      context,
      4,
      "Receiver interval: {}, preserves_type_context: {}",
      interval,
      is_this_call);
  return CallClassIntervalContext(
      interval, /* preserves_type_context */ is_this_call);
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

bool get_is_this_call(
    const RegisterMemoryLocationsMap& register_memory_locations_map,
    const IRInstruction* instruction) {
  if (instruction->opcode() != OPCODE_INVOKE_VIRTUAL) {
    return false;
  }

  // Virtual calls have at least one argument (the receiver).
  mt_assert(instruction->srcs_size() > 0);

  auto receiver_register = instruction->src(0);
  const auto* receiver_memory_location_singleton =
      register_memory_locations_map.at(receiver_register).singleton();
  if (receiver_memory_location_singleton == nullptr) {
    return false;
  }

  const auto* receiver_memory_location = *receiver_memory_location_singleton;
  if (receiver_memory_location == nullptr) {
    return false;
  }

  return receiver_memory_location->is<ThisParameterMemoryLocation>();
}

CalleeModel get_callee(
    const MethodContext* context,
    const IRInstruction* instruction,
    const DexPosition* MT_NULLABLE dex_position,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    bool is_this_call) {
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

  auto class_interval_context =
      get_type_context(context, instruction, is_this_call);
  auto model = context->model_at_callsite(
      call_target,
      position,
      source_register_types,
      source_constant_arguments,
      class_interval_context);
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
      /* source_constant_arguments */ {},
      /* class_interval_context */ CallClassIntervalContext());
  LOG_OR_DUMP(context, 4, "Callee model: {}", model);

  return CalleeModel{
      resolved_base_callee->dex_method(),
      resolved_base_callee,
      position,
      callee.call_target.call_index(),
      std::move(model)};
}

namespace {

bool is_safe_to_inline(
    const MethodContext* context,
    const CalleeModel& callee,
    const AccessPath& input,
    const Kind* output_kind,
    const Path& output_path) {
  if (!callee.model.generations().is_bottom()) {
    LOG_OR_DUMP(
        context,
        4,
        "Could not inline call because callee model has generations");
    return false;
  }
  if (!callee.model.sinks().is_bottom()) {
    LOG_OR_DUMP(
        context, 4, "Could not inline call because callee model has sinks");
    return false;
  }
  if (callee.model.add_via_obscure_feature()) {
    LOG_OR_DUMP(
        context,
        4,
        "Could not inline call because callee model has add-via-obscure");
    return false;
  }
  if (callee.model.has_add_features_to_arguments()) {
    LOG_OR_DUMP(
        context,
        4,
        "Could not inline call because callee model has add-features-to-arguments");
    return false;
  }
  if (!callee.model.propagations().leq(TaintAccessPathTree(
          {{/* input */ input,
            Taint::propagation(PropagationConfig(
                /* input_path */ input,
                /* kind */ output_kind,
                /* output_paths */
                PathTreeDomain{{output_path, CollapseDepth::zero()}},
                /* inferred_features */ {},
                /* locally_inferred_features */ {},
                /* user_features */ FeatureSet::bottom()))}}))) {
    LOG_OR_DUMP(
        context,
        4,
        "Could not inline call because callee model has extra propagations");
    return false;
  }

  return true;
}

/* Returns the memory location for the given parameter access path at the given
 * invoke instruction. Returns nullptr if there are multiple possible memory
 * locations.
 */
MemoryLocation* MT_NULLABLE memory_location_for_invoke_parameter(
    const IRInstruction* instruction,
    const RegisterMemoryLocationsMap& register_memory_locations_map,
    const AccessPath& parameter) {
  mt_assert(parameter.root().is_argument());
  auto register_id = instruction->src(parameter.root().parameter_position());
  auto memory_locations = register_memory_locations_map.at(register_id);
  auto* memory_location_singleton = memory_locations.singleton();
  if (memory_location_singleton == nullptr) {
    return nullptr;
  }
  MemoryLocation* memory_location = *memory_location_singleton;
  return memory_location->make_field(parameter.path());
}

} // namespace

MemoryLocation* MT_NULLABLE try_inline_invoke_as_getter(
    const MethodContext* context,
    const RegisterMemoryLocationsMap& register_memory_locations_map,
    const IRInstruction* instruction,
    const CalleeModel& callee) {
  auto access_path = callee.model.inline_as_getter().get_constant();
  if (!access_path) {
    return nullptr;
  }

  auto* memory_location = memory_location_for_invoke_parameter(
      instruction, register_memory_locations_map, *access_path);
  if (memory_location == nullptr) {
    LOG_OR_DUMP(
        context,
        4,
        "Could not inline call because parameter {} points to multiple memory locations",
        access_path->root());
    return nullptr;
  }

  if (!is_safe_to_inline(
          context,
          callee,
          /* input */ *access_path,
          /* output */ context->kind_factory.local_return(),
          /* output_path */ Path{})) {
    return nullptr;
  }

  return memory_location;
}

std::optional<SetterInlineMemoryLocations> try_inline_invoke_as_setter(
    const MethodContext* context,
    const RegisterMemoryLocationsMap& register_memory_locations_map,
    const IRInstruction* instruction,
    const CalleeModel& callee) {
  auto setter = callee.model.inline_as_setter().get_constant();
  if (!setter) {
    return std::nullopt;
  }

  auto* target_memory_location = memory_location_for_invoke_parameter(
      instruction, register_memory_locations_map, setter->target());
  if (target_memory_location == nullptr) {
    LOG_OR_DUMP(
        context,
        4,
        "Could not inline call because target {} points to multiple memory locations",
        setter->target());
    return std::nullopt;
  }

  auto* value_memory_location = memory_location_for_invoke_parameter(
      instruction, register_memory_locations_map, setter->value());
  if (value_memory_location == nullptr) {
    LOG_OR_DUMP(
        context,
        4,
        "Could not inline call because value {} points to multiple memory locations",
        setter->value());
    return std::nullopt;
  }

  if (!is_safe_to_inline(
          context,
          callee,
          /* input */ setter->value(),
          /* output */
          context->kind_factory.local_argument(
              setter->target().root().parameter_position()),
          /* output_path */ setter->target().path())) {
    return std::nullopt;
  }

  auto* position = context->positions.get(
      callee.position, setter->value().root(), instruction);

  return SetterInlineMemoryLocations{
      target_memory_location, value_memory_location, position};
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

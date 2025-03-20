/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/ForwardAliasTransfer.h>
#include <mariana-trench/KotlinHeuristics.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/TransferCall.h>

namespace marianatrench {

bool ForwardAliasTransfer::analyze_default(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
  log_instruction(context, instruction);

  // Assign the result register to a new fresh memory location.
  if (instruction->has_dest()) {
    auto* memory_location = context->memory_factory.make_location(instruction);
    LOG_OR_DUMP(
        context,
        4,
        "Setting register {} to {}",
        instruction->dest(),
        show(memory_location));
    environment->assign(instruction->dest(), memory_location);
  } else if (instruction->has_move_result_any()) {
    auto* memory_location = context->memory_factory.make_location(instruction);
    LOG_OR_DUMP(
        context, 4, "Setting result register to {}", show(memory_location));
    environment->assign(k_result_register, memory_location);
  } else {
    return false;
  }

  return false;
}

bool ForwardAliasTransfer::analyze_check_cast(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs_size() == 1);

  // check-cast acts as a passthrough for the input register.
  auto input_memory_locations =
      environment->memory_locations(instruction->src(0));
  LOG_OR_DUMP(
      context, 4, "Setting result register to {}", input_memory_locations);
  environment->assign(k_result_register, input_memory_locations);

  return false;
}

bool ForwardAliasTransfer::analyze_iget(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs_size() == 1);
  mt_assert(instruction->has_field());

  // Read source memory locations that stores the fields.
  auto source_memory_locations =
      environment->memory_locations(instruction->src(0));
  // Build the widening resolver from the entry state.
  auto widening_resolver = environment->make_widening_resolver();

  const auto* field = instruction->get_field()->get_name();
  MemoryLocationsDomain field_memory_locations;

  // Check if root memory location exists for this field access and
  // create otherwise.
  for (auto* memory_location : source_memory_locations.elements()) {
    auto* field_memory_location = memory_location->make_field(field);
    field_memory_locations.add(field_memory_location);
    if (environment->points_to(field_memory_location).is_bottom()) {
      auto* root_memory_location =
          context->memory_factory.make_location(instruction);

      environment->write(
          widening_resolver,
          memory_location,
          field,
          PointsToSet{root_memory_location},
          UpdateKind::Strong);

      LOG_OR_DUMP(
          context,
          4,
          "Updated points-to tree at memory location: {} field: {} with new root memory location {}",
          show(root_memory_location),
          show(memory_location),
          show(field));
    }
  }

  LOG_OR_DUMP(
      context, 4, "Setting result register to {}", field_memory_locations);
  environment->assign(k_result_register, field_memory_locations);

  return false;
}

bool ForwardAliasTransfer::analyze_sget(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs_size() == 0);
  mt_assert(instruction->has_field());

  auto memory_location = context->memory_factory.make_location(instruction);
  LOG_OR_DUMP(context, 4, "Setting result register to {}", *memory_location);
  environment->assign(k_result_register, memory_location);

  return false;
}

namespace {

MemoryLocation* MT_NULLABLE try_alias_this_location(
    MethodContext* context,
    ForwardAliasEnvironment* environment,
    const CalleeModel& callee,
    const IRInstruction* instruction) {
  if (!callee.model.alias_memory_location_on_invoke()) {
    return nullptr;
  }

  if (callee.resolved_base_method && callee.resolved_base_method->is_static()) {
    return nullptr;
  }

  auto register_id = instruction->src(0);
  auto memory_locations = environment->memory_locations(register_id);

  auto* memory_location_singleton = memory_locations.singleton();
  if (memory_location_singleton == nullptr) {
    return nullptr;
  }

  auto* memory_location = *memory_location_singleton;
  LOG_OR_DUMP(
      context,
      4,
      "Method invoke aliasing existing memory location {}",
      show(memory_location));

  return memory_location;
}

MemoryLocationsDomain invoke_result_memory_location(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
  auto register_memory_locations_map = memory_location_map_from_environment(
      environment->memory_location_environment(), instruction);

  auto callee = get_callee(
      context,
      instruction,
      environment->last_position(),
      register_memory_locations_map);

  if (callee.resolved_base_method &&
      callee.resolved_base_method->returns_void()) {
    return MemoryLocationsDomain::bottom();
  }

  auto* memory_location = try_inline_invoke_as_getter(
      context, register_memory_locations_map, instruction, callee);
  if (memory_location != nullptr) {
    LOG_OR_DUMP(context, 4, "Inlining method call");
    return MemoryLocationsDomain{memory_location};
  }

  memory_location =
      try_alias_this_location(context, environment, callee, instruction);
  if (memory_location != nullptr) {
    return MemoryLocationsDomain{memory_location};
  }

  memory_location = context->memory_factory.make_location(instruction);
  return MemoryLocationsDomain{memory_location};
}

/**
 * Helper to check if the output path is eligible aliasing propagation
 */
const DexString* MT_NULLABLE
maybe_get_aliasing_output_path_element(const Path& output_path) {
  // Only considering `this$n` access path.
  if (output_path.size() != 1) {
    return nullptr;
  }

  auto path_element = *output_path.begin();
  if (!path_element.is_field()) {
    return nullptr;
  }

  // Currently, just check for this$
  auto* field_name = path_element.name();
  mt_assert(field_name != nullptr);
  if (!boost::starts_with(field_name->str(), "this$")) {
    return nullptr;
  }

  return field_name;
}

/**
 * Helper to check if the input path and the corresponding source memory
 * location is eligible for aliasing propagation
 */
RootMemoryLocation* MT_NULLABLE
maybe_get_aliasing_propagation_source_memory_location(
    const IRInstruction* instruction,
    const ForwardAliasEnvironment* environment,
    const AccessPath& input_path) {
  // For now, only consider Argument input roots without paths.
  if (!input_path.root().is_argument() || !input_path.path().empty()) {
    return nullptr;
  }

  // Source memory locations
  auto input_register_id =
      instruction->src(input_path.root().parameter_position());
  auto source_memory_locations =
      environment->memory_locations(input_register_id);

  // Check if the source memory location is a singleton and
  // RootMemoryLocation, or else skip for now.
  auto* source_memory_location_singleton = source_memory_locations.singleton();
  if (source_memory_location_singleton == nullptr) {
    return nullptr;
  }

  return (*source_memory_location_singleton)->as<RootMemoryLocation>();
}

/**
 * Creates an alias to the memory locations in the `points_to_set` from
 * `target_memory_locations` at path `field_name`.
 */
void create_alias(
    MethodContext* context,
    ForwardAliasEnvironment* environment,
    const WideningPointsToResolver& widening_resolver,
    PointsToSet points_to_set,
    const MemoryLocationsDomain& target_memory_locations,
    const DexString* field_name) {
  if (points_to_set.is_bottom()) {
    return;
  }

  bool is_singleton = target_memory_locations.singleton() != nullptr;

  for (auto* memory_location : target_memory_locations.elements()) {
    // TODO: T142954672 FieldMemoryLocation can widen consecutive duplicate
    // paths. Identify when widening and always weak update for widened memory
    // locations.
    auto* field_memory_location = memory_location->make_field(field_name);
    points_to_set.add_locally_inferred_features(
        get_field_features(context, field_memory_location));

    LOG_OR_DUMP(
        context,
        4,
        "{} updating PointsToTree at {} with {}",
        is_singleton ? "Strong" : "Weak",
        show(field_memory_location),
        points_to_set);

    environment->write(
        widening_resolver,
        memory_location,
        field_name,
        points_to_set,
        is_singleton ? UpdateKind::Strong : UpdateKind::Weak);
  }
}

void apply_aliasing_propagations(
    MethodContext* context,
    ForwardAliasEnvironment* environment,
    const WideningPointsToResolver& widening_resolver,
    const IRInstruction* instruction) {
  auto register_memory_locations_map = memory_location_map_from_environment(
      environment->memory_location_environment(), instruction);
  auto callee = get_callee(
      context,
      instruction,
      environment->last_position(),
      register_memory_locations_map);

  if (callee.resolved_base_method == nullptr ||
      callee.resolved_base_method->is_static()) {
    // Cannot capture parent scope in .this$n
    return;
  }

  LOG_OR_DUMP(
      context,
      4,
      "Processing propagations for call to `{}`",
      show(callee.method_reference));

  // Process the aliasing propagations.
  // Currently only processing propagations to specific access path `.this$n`.
  for (const auto& binding : callee.model.propagations().elements()) {
    const AccessPath& input_path = binding.first;
    auto* aliasing_source_memory_location =
        maybe_get_aliasing_propagation_source_memory_location(
            instruction, environment, input_path);

    if (aliasing_source_memory_location == nullptr) {
      // Nothing to alias
      continue;
    }

    LOG_OR_DUMP(
        context,
        4,
        "Memory location {} at input path {} of propagation can be aliased",
        show(aliasing_source_memory_location),
        input_path);

    const Taint& propagations = binding.second;
    propagations.visit_frames([context,
                               instruction,
                               environment,
                               &input_path,
                               aliasing_source_memory_location,
                               &widening_resolver](
                                  const CallInfo& /* call_info */,
                                  const Frame& propagation) {
      // Only handle propgation to Argument(0).
      auto output_root = propagation.propagation_kind()->root();
      if (!(output_root.is_argument() &&
            output_root.parameter_position() == 0)) {
        return;
      }

      for (const auto& [output_path, collapse_depth] :
           propagation.output_paths().elements()) {
        const auto* output_path_element =
            maybe_get_aliasing_output_path_element(output_path);
        if (output_path_element == nullptr) {
          // Nothing to alias.
          continue;
        }

        LOG_OR_DUMP(
            context,
            4,
            "Output root {} at path {} of propagation aliases input path: {}",
            output_root,
            show(output_path_element),
            input_path);

        // Create the source points-to set
        PointsToSet points_to_set{
            aliasing_source_memory_location,
            AliasingProperties::with_collapse_depth(collapse_depth)};

        // Retrieve the target memory locations where the alias is to be
        // created.
        auto output_register_id =
            instruction->src(output_root.parameter_position());
        auto target_memory_locations =
            environment->memory_locations(output_register_id);

        create_alias(
            context,
            environment,
            widening_resolver,
            std::move(points_to_set),
            target_memory_locations,
            output_path_element);
      }
    });
  }
}

} // namespace

bool ForwardAliasTransfer::analyze_invoke(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
  log_instruction(context, instruction);

  // Build the widening resolver from the entry state.
  auto widening_resolver = environment->make_widening_resolver();

  auto memory_locations =
      invoke_result_memory_location(context, instruction, environment);

  // Set result register
  LOG_OR_DUMP(context, 4, "Setting result register to {}", memory_locations);
  environment->assign(k_result_register, memory_locations);

  apply_aliasing_propagations(
      context, environment, widening_resolver, instruction);

  return false;
}

SetterAccessPathConstantDomain infer_field_write(
    MethodContext* context,
    const IRInstruction* instruction,
    const ForwardAliasEnvironment* environment) {
  auto value_memory_locations =
      environment->memory_locations(instruction->src(0));
  auto* value_memory_location_singleton = value_memory_locations.singleton();
  if (value_memory_location_singleton == nullptr) {
    return SetterAccessPathConstantDomain::top();
  }
  MemoryLocation* value_memory_location = *value_memory_location_singleton;
  auto value_access_path = value_memory_location->access_path();

  if (!value_access_path) {
    return SetterAccessPathConstantDomain::top();
  }

  // If size of access_path is greater than the max input path size of the
  // propagation taint tree, it is not safe to inline_as_setter and can lead
  // to invalid trees.
  if (value_access_path->path().size() >
      context->heuristics.propagation_max_input_path_size()) {
    return SetterAccessPathConstantDomain::top();
  }

  auto target_memory_locations =
      environment->memory_locations(instruction->src(1));
  auto* target_memory_location_singleton = target_memory_locations.singleton();
  if (target_memory_location_singleton == nullptr) {
    return SetterAccessPathConstantDomain::top();
  }
  MemoryLocation* target_memory_location = *target_memory_location_singleton;
  auto target_access_path = target_memory_location->access_path();
  if (!target_access_path) {
    return SetterAccessPathConstantDomain::top();
  }

  const auto* field_name = instruction->get_field()->get_name();
  target_access_path->append(PathElement::field(field_name));

  // If size of access_path is greater than the max output path size of the
  // propagation taint tree, it is not safe to inline_as_setter and can lead
  // to invalid trees.
  if (target_access_path->path().size() >
      context->heuristics.propagation_max_output_path_size()) {
    return SetterAccessPathConstantDomain::top();
  }

  auto setter = SetterAccessPath(
      /* target */ *target_access_path,
      /* value */ *value_access_path);
  LOG_OR_DUMP(context, 4, "Instruction can be inlined as {}", setter);
  return SetterAccessPathConstantDomain(setter);
}

bool ForwardAliasTransfer::analyze_iput(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
  log_instruction(context, instruction);

  // Update the points-to environment
  auto source_memory_locations =
      environment->memory_locations(instruction->src(0));
  // Build the widening resolver from the entry state.
  auto widening_resolver = environment->make_widening_resolver();

  const auto* field_name = instruction->get_field()->get_name();
  auto target_memory_locations =
      environment->memory_locations(instruction->src(1));

  auto points_to = environment->points_to(source_memory_locations);
  auto* position = context->positions.get(
      context->method(),
      environment->last_position(),
      Root(Root::Kind::Return),
      instruction);
  points_to.add_local_position(position);

  create_alias(
      context,
      environment,
      widening_resolver,
      std::move(points_to),
      target_memory_locations,
      field_name);

  // Handle field_write to infer inline as setter.
  const auto& current_field_write = environment->field_write();
  if (!current_field_write.is_bottom()) {
    // We have already seen a `iput` before.
    environment->set_field_write(SetterAccessPathConstantDomain::top());
    return false;
  }

  environment->set_field_write(
      infer_field_write(context, instruction, environment));

  return false;
}

bool ForwardAliasTransfer::analyze_sput(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* /* environment */) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs_size() == 1);
  mt_assert(instruction->has_field());

  // This is a no-op.
  return false;
}

bool ForwardAliasTransfer::analyze_load_param(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
  log_instruction(context, instruction);

  auto abstract_parameter = environment->last_parameter_loaded();
  if (!abstract_parameter.is_value()) {
    ERROR_OR_DUMP(context, 1, "Failed to deduce the parameter of a load");
    return false;
  }
  auto parameter_position = *abstract_parameter.get_constant();
  environment->increment_last_parameter_loaded();

  // Create a memory location that represents the argument.
  auto memory_location =
      context->memory_factory.make_parameter(parameter_position);
  LOG_OR_DUMP(
      context,
      4,
      "Setting register {} to {}",
      instruction->dest(),
      show(memory_location));
  environment->assign(instruction->dest(), memory_location);

  return false;
}

bool ForwardAliasTransfer::analyze_move(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs_size() == 1);

  auto memory_locations =
      environment->memory_locations(/* register */ instruction->src(0));
  LOG_OR_DUMP(
      context,
      4,
      "Setting register {} to {}",
      instruction->dest(),
      memory_locations);
  environment->assign(instruction->dest(), memory_locations);

  return false;
}

bool ForwardAliasTransfer::analyze_move_result(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
  log_instruction(context, instruction);

  auto memory_locations = environment->memory_locations(k_result_register);
  LOG_OR_DUMP(
      context,
      4,
      "Setting register {} to {}",
      instruction->dest(),
      memory_locations);
  environment->assign(instruction->dest(), memory_locations);

  LOG_OR_DUMP(context, 4, "Resetting the result register");
  environment->assign(k_result_register, MemoryLocationsDomain::bottom());

  return false;
}

bool ForwardAliasTransfer::analyze_aget(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs_size() == 2);

  // aget v0, v1 reads the value from array at v0 at index v1.
  // We use a single memory location for the array and its elements and ignore
  // the index.
  auto memory_locations =
      environment->memory_locations(/* register */ instruction->src(0));
  LOG_OR_DUMP(context, 4, "Setting result register to {}", memory_locations);
  environment->assign(k_result_register, memory_locations);

  return false;
}

namespace {

bool has_side_effect_with_heuristics(
    const MethodContext* context,
    const MethodItemEntry& instruction) {
  switch (instruction.type) {
    case MFLOW_OPCODE:
      switch (instruction.insn->opcode()) {
        case IOPCODE_LOAD_PARAM:
        case IOPCODE_LOAD_PARAM_OBJECT:
        case IOPCODE_LOAD_PARAM_WIDE:
        case OPCODE_NOP:
        case OPCODE_MOVE:
        case OPCODE_MOVE_WIDE:
        case OPCODE_MOVE_OBJECT:
        case OPCODE_MOVE_RESULT:
        case OPCODE_MOVE_RESULT_WIDE:
        case OPCODE_MOVE_RESULT_OBJECT:
        case IOPCODE_MOVE_RESULT_PSEUDO:
        case IOPCODE_MOVE_RESULT_PSEUDO_OBJECT:
        case IOPCODE_MOVE_RESULT_PSEUDO_WIDE:
        case OPCODE_RETURN_VOID:
        case OPCODE_RETURN:
        case OPCODE_RETURN_WIDE:
        case OPCODE_RETURN_OBJECT:
        case OPCODE_CONST:
        case OPCODE_CONST_WIDE:
        case OPCODE_IGET:
        case OPCODE_IGET_WIDE:
        case OPCODE_IGET_OBJECT:
        case OPCODE_IGET_BOOLEAN:
        case OPCODE_IGET_BYTE:
        case OPCODE_IGET_CHAR:
        case OPCODE_IGET_SHORT:
          return false;

        case OPCODE_CONST_STRING:
          return KotlinHeuristics::const_string_has_side_effect(
              instruction.insn);

        case OPCODE_INVOKE_STATIC: {
          auto call_target =
              context->call_graph.callee(context->method(), instruction.insn);
          if (auto* resolved_callee = call_target.resolved_base_callee()) {
            return KotlinHeuristics::method_has_side_effects(
                resolved_callee->dex_method());
          }
          // Call could not be resolved. Default to has side-effects.
          return true;
        }

        default:
          return true;
      }
      break;
    case MFLOW_DEBUG:
    case MFLOW_POSITION:
    case MFLOW_FALLTHROUGH:
      return false;
    default:
      return true;
  }
}

bool is_iput_instruction(const MethodItemEntry& instruction) {
  return instruction.type == MFLOW_OPCODE &&
      opcode::is_an_iput(instruction.insn->opcode());
}

bool is_safe_to_inline(MethodContext* context, bool allow_iput) {
  if (context->previous_model.has_global_propagation_sanitizer()) {
    LOG_OR_DUMP(
        context,
        4,
        "Method has global propagation sanitizers, it cannot be inlined.");
    return false;
  }

  // Check if the method has any side effect.
  const auto* code = context->method()->get_code();
  mt_assert(code != nullptr);
  const auto& cfg = code->cfg();
  if (cfg.blocks().size() != 1) {
    // There could be multiple return statements.
    LOG_OR_DUMP(
        context, 4, "Method has multiple basic blocks, it cannot be inlined.");
    return false;
  }

  auto* entry_block = cfg.entry_block();
  auto found = std::find_if(
      entry_block->begin(),
      entry_block->end(),
      [allow_iput, context](const auto& instruction) {
        return has_side_effect_with_heuristics(context, instruction) &&
            (!allow_iput || !is_iput_instruction(instruction));
      });
  if (found != entry_block->end()) {
    LOG_OR_DUMP(
        context,
        4,
        "Method has an instruction with possible side effects: {}, it cannot be inlined.",
        show(*found));
    return false;
  }

  return true;
}

AccessPathConstantDomain infer_inline_as_getter(
    MethodContext* context,
    const MemoryLocationsDomain& memory_locations) {
  if (!is_safe_to_inline(context, /* allow_iput */ false)) {
    return AccessPathConstantDomain::top();
  }

  // Check if we are returning an argument access path.
  auto* memory_location_singleton = memory_locations.singleton();
  if (memory_location_singleton == nullptr) {
    return AccessPathConstantDomain::top();
  }

  MemoryLocation* memory_location = *memory_location_singleton;
  auto access_path = memory_location->access_path();
  if (!access_path) {
    return AccessPathConstantDomain::top();
  }

  // If size of access_path is greater than the max input path size of the
  // propagation taint tree, it is not safe to inline_as_getter and can lead to
  // invalid trees.
  if (access_path->path().size() >
      context->heuristics.propagation_max_input_path_size()) {
    return AccessPathConstantDomain::top();
  }

  LOG_OR_DUMP(
      context, 4, "Method can be inlined as a getter for {}", *access_path);
  return AccessPathConstantDomain(*access_path);
}

SetterAccessPathConstantDomain infer_inline_as_setter(
    MethodContext* context,
    const ForwardAliasEnvironment* environment) {
  if (!is_safe_to_inline(context, /* allow_iput */ true)) {
    return SetterAccessPathConstantDomain::top();
  }

  auto field_write = environment->field_write().get_constant();
  if (!field_write) {
    return SetterAccessPathConstantDomain::top();
  }

  LOG_OR_DUMP(context, 4, "Method can be inlined as setter {}", *field_write);
  return SetterAccessPathConstantDomain(*field_write);
}

} // namespace

bool ForwardAliasTransfer::analyze_return(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs_size() <= 1);

  if (instruction->srcs_size() == 1) {
    auto register_id = instruction->src(0);
    auto memory_locations = environment->memory_locations(register_id);
    context->new_model.set_inline_as_getter(
        infer_inline_as_getter(context, memory_locations));
    context->new_model.set_inline_as_setter(
        SetterAccessPathConstantDomain::top());
  } else if (instruction->srcs_size() == 0) {
    context->new_model.set_inline_as_setter(
        infer_inline_as_setter(context, environment));
    context->new_model.set_inline_as_getter(AccessPathConstantDomain::top());
  }

  return false;
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/ForwardAliasTransfer.h>
#include <mariana-trench/Log.h>
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

  // We want to consider the cast as creating a different memory location,
  // so it can have a different taint.
  auto memory_location = context->memory_factory.make_location(instruction);
  LOG_OR_DUMP(
      context, 4, "Setting result register to {}", show(memory_location));
  environment->assign(k_result_register, memory_location);

  return false;
}

bool ForwardAliasTransfer::analyze_iget(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
  log_instruction(context, instruction);
  mt_assert(instruction->srcs_size() == 1);
  mt_assert(instruction->has_field());

  // Read source memory locations that represents the field.
  auto memory_locations = environment->memory_locations(
      /* register */ instruction->src(0),
      /* field */ instruction->get_field()->get_name());
  LOG_OR_DUMP(context, 4, "Setting result register to {}", memory_locations);
  environment->assign(k_result_register, memory_locations);

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
      get_source_register_types(context, instruction),
      get_source_constant_arguments(
          register_memory_locations_map, instruction));

  if (callee.resolved_base_method &&
      callee.resolved_base_method->returns_void()) {
    return MemoryLocationsDomain::bottom();
  }

  auto* memory_location = try_inline_invoke(
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

} // namespace

bool ForwardAliasTransfer::analyze_invoke(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
  log_instruction(context, instruction);

  auto memory_locations =
      invoke_result_memory_location(context, instruction, environment);
  LOG_OR_DUMP(context, 4, "Setting result register to {}", memory_locations);
  environment->assign(k_result_register, memory_locations);
  return false;
}

bool ForwardAliasTransfer::analyze_iput(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
  log_instruction(context, instruction);

  // This is a no-op.
  return false;
}

bool ForwardAliasTransfer::analyze_sput(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
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

  // We use a single memory location for the array and its elements.
  auto memory_locations =
      environment->memory_locations(/* register */ instruction->src(0));
  LOG_OR_DUMP(context, 4, "Setting result register to {}", memory_locations);
  environment->assign(k_result_register, memory_locations);

  return false;
}

namespace {

bool has_side_effect(const MethodItemEntry& instruction) {
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

// Infer whether the method could be inlined.
AccessPathConstantDomain infer_inline_as(
    MethodContext* context,
    const MemoryLocationsDomain& memory_locations) {
  if (context->model.has_global_propagation_sanitizer()) {
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

  LOG_OR_DUMP(
      context, 4, "Instruction returns the access path: {}", *access_path);

  // Check if the method has any side effect.
  const auto* code = context->method()->get_code();
  mt_assert(code != nullptr);
  const auto& cfg = code->cfg();
  if (cfg.blocks().size() != 1) {
    // There could be multiple return statements.
    LOG_OR_DUMP(
        context, 4, "Method has multiple basic blocks, it cannot be inlined.");
    return AccessPathConstantDomain::top();
  }

  auto* entry_block = cfg.entry_block();
  auto found =
      std::find_if(entry_block->begin(), entry_block->end(), has_side_effect);
  if (found != entry_block->end()) {
    LOG_OR_DUMP(
        context,
        4,
        "Method has an instruction with possible side effects: {}, it cannot be inlined.",
        show(*found));
    return AccessPathConstantDomain::top();
  }

  LOG_OR_DUMP(context, 4, "Method can be inlined as {}", *access_path);
  return AccessPathConstantDomain(*access_path);
}

} // namespace

bool ForwardAliasTransfer::analyze_return(
    MethodContext* context,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
  log_instruction(context, instruction);

  for (auto register_id : instruction->srcs()) {
    auto memory_locations = environment->memory_locations(register_id);
    context->model.set_inline_as(infer_inline_as(context, memory_locations));
  }

  return false;
}

} // namespace marianatrench

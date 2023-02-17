/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/ForwardAliasFixpoint.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/TransferCall.h>

namespace marianatrench {

ForwardAliasFixpoint::ForwardAliasFixpoint(
    MethodContext& context,
    const cfg::ControlFlowGraph& cfg,
    InstructionAnalyzer<ForwardAliasEnvironment> instruction_analyzer)
    : MonotonicFixpointIterator(cfg, cfg.num_blocks()),
      context_(context),
      instruction_analyzer_(std::move(instruction_analyzer)) {}

ForwardAliasFixpoint::~ForwardAliasFixpoint() {}

namespace {

void analyze_instruction(
    MethodContext& context,
    const InstructionAnalyzer<ForwardAliasEnvironment>& instruction_analyzer,
    const IRInstruction* instruction,
    ForwardAliasEnvironment* environment) {
  MemoryLocationEnvironment memory_location_environment =
      environment->memory_location_environment();

  // Calls into ForwardAliasTransfer::analyze_xxx
  instruction_analyzer(instruction, environment);

  std::optional<MemoryLocationsDomain> result_memory_locations = std::nullopt;
  if (instruction->has_dest()) {
    result_memory_locations =
        environment->memory_locations(instruction->dest());
  } else if (instruction->has_move_result_any()) {
    result_memory_locations = environment->memory_locations(k_result_register);
  }

  context.aliasing.store(
      instruction,
      InstructionAliasResults{
          memory_location_environment,
          result_memory_locations,
          environment->last_position()});
}

} // namespace

void ForwardAliasFixpoint::analyze_node(
    const NodeId& block,
    ForwardAliasEnvironment* environment) const {
  LOG(4, "Analyzing block {}\n{}", block->id(), *environment);
  for (auto& instruction : *block) {
    switch (instruction.type) {
      case MFLOW_OPCODE:
        analyze_instruction(
            context_, instruction_analyzer_, instruction.insn, environment);
        break;
      case MFLOW_POSITION:
        environment->set_last_position(instruction.pos.get());
        break;
      default:
        break;
    }
  }
}

ForwardAliasEnvironment ForwardAliasFixpoint::analyze_edge(
    const EdgeId& /*edge*/,
    const ForwardAliasEnvironment& environment) const {
  return environment;
}

} // namespace marianatrench

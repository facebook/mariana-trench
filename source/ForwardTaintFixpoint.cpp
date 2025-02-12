/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/ForwardTaintFixpoint.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

ForwardTaintFixpoint::ForwardTaintFixpoint(
    const MethodContext& method_context,
    const cfg::ControlFlowGraph& cfg,
    InstructionAnalyzer<ForwardTaintEnvironment> instruction_analyzer)
    : MonotonicFixpointIterator(cfg, cfg.num_blocks()),
      context_(method_context),
      instruction_analyzer_(std::move(instruction_analyzer)) {}

ForwardTaintFixpoint::~ForwardTaintFixpoint() {}

void ForwardTaintFixpoint::analyze_node(
    const NodeId& block,
    ForwardTaintEnvironment* taint) const {
  LOG(4, "Analyzing block {}\n{}", block->id(), *taint);

  auto duration = timer_.duration_in_seconds();
  auto maximum_method_analysis_time =
      context_.options.maximum_method_analysis_time().value_or(
          std::numeric_limits<int>::max());
  if (duration > maximum_method_analysis_time) {
    throw TimeoutError(
        fmt::format(
            "Forward taint analysis of `{}` exceeded timeout of {}s.",
            context_.method()->show(),
            maximum_method_analysis_time),
        duration);
  }

  for (const auto& instruction : *block) {
    switch (instruction.type) {
      case MFLOW_OPCODE:
        instruction_analyzer_(instruction.insn, taint);
        break;
      default:
        break;
    }
  }
}

ForwardTaintEnvironment ForwardTaintFixpoint::analyze_edge(
    const EdgeId& /*edge*/,
    const ForwardTaintEnvironment& taint) const {
  return taint;
}

} // namespace marianatrench

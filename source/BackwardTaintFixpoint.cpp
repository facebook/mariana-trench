/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/BackwardTaintFixpoint.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

BackwardTaintFixpoint::BackwardTaintFixpoint(
    const MethodContext& method_context,
    const cfg::ControlFlowGraph& cfg,
    InstructionAnalyzer<BackwardTaintEnvironment> instruction_analyzer)
    : MonotonicFixpointIterator(cfg, cfg.num_blocks()),
      context_(method_context),
      instruction_analyzer_(instruction_analyzer) {}

BackwardTaintFixpoint::~BackwardTaintFixpoint() {}

void BackwardTaintFixpoint::analyze_node(
    const NodeId& block,
    BackwardTaintEnvironment* taint) const {
  LOG(4, "Analyzing block {}\n{}", block->id(), *taint);

  auto duration = timer_.duration_in_seconds();
  auto maximum_method_analysis_time =
      context_.options.maximum_method_analysis_time().value_or(
          std::numeric_limits<int>::max());
  if (duration > maximum_method_analysis_time) {
    throw TimeoutError(
        fmt::format(
            "Backward taint analysis of `{}` exceeded timeout of {}s.",
            context_.method()->show(),
            maximum_method_analysis_time),
        duration);
  }

  for (auto it = block->rbegin(); it != block->rend(); ++it) {
    if (it->type == MFLOW_OPCODE) {
      instruction_analyzer_(it->insn, taint);
    }
  }
}

BackwardTaintEnvironment BackwardTaintFixpoint::analyze_edge(
    const EdgeId& /*edge*/,
    const BackwardTaintEnvironment& taint) const {
  return taint;
}

} // namespace marianatrench

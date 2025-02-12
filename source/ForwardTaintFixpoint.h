/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sparta/MonotonicFixpointIterator.h>

#include <ControlFlow.h>
#include <InstructionAnalyzer.h>

#include <mariana-trench/ForwardTaintEnvironment.h>
#include <mariana-trench/MethodContext.h>
#include <mariana-trench/Timer.h>

namespace marianatrench {

class ForwardTaintFixpoint final : public sparta::MonotonicFixpointIterator<
                                       cfg::GraphInterface,
                                       ForwardTaintEnvironment> {
 public:
  ForwardTaintFixpoint(
      const MethodContext& method_context,
      const cfg::ControlFlowGraph& cfg,
      InstructionAnalyzer<ForwardTaintEnvironment> instruction_analyzer);

  ~ForwardTaintFixpoint();

  void analyze_node(const NodeId& block, ForwardTaintEnvironment* taint)
      const override;

  ForwardTaintEnvironment analyze_edge(
      const EdgeId& edge,
      const ForwardTaintEnvironment& taint) const override;

  const Timer& timer() const {
    return timer_;
  }

 private:
  const MethodContext& context_;
  InstructionAnalyzer<ForwardTaintEnvironment> instruction_analyzer_;

  // Timer tracking approximately when the analysis was started.
  // Default constructed.
  Timer timer_;
};

} // namespace marianatrench

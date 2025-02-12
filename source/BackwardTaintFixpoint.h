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

#include <mariana-trench/BackwardTaintEnvironment.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Timer.h>

namespace marianatrench {

class BackwardTaintFixpoint final
    : public sparta::MonotonicFixpointIterator<
          sparta::BackwardsFixpointIterationAdaptor<cfg::GraphInterface>,
          BackwardTaintEnvironment> {
 public:
  BackwardTaintFixpoint(
      const MethodContext& context,
      const cfg::ControlFlowGraph& cfg,
      InstructionAnalyzer<BackwardTaintEnvironment> instruction_analyzer);

  ~BackwardTaintFixpoint();

  void analyze_node(const NodeId& block, BackwardTaintEnvironment* taint)
      const override;

  BackwardTaintEnvironment analyze_edge(
      const EdgeId& edge,
      const BackwardTaintEnvironment& taint) const override;

  const Timer& timer() const {
    return timer_;
  }

 private:
  const MethodContext& context_;
  InstructionAnalyzer<BackwardTaintEnvironment> instruction_analyzer_;

  // Timer tracking approximately when the analysis was started.
  // Default constructed.
  Timer timer_;
};

} // namespace marianatrench

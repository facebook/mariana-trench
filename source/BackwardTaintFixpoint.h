/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ControlFlow.h>
#include <InstructionAnalyzer.h>
#include <MonotonicFixpointIterator.h>

#include <mariana-trench/BackwardTaintEnvironment.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

class BackwardTaintFixpoint final
    : public sparta::MonotonicFixpointIterator<
          sparta::BackwardsFixpointIterationAdaptor<cfg::GraphInterface>,
          BackwardTaintEnvironment> {
 public:
  BackwardTaintFixpoint(
      const cfg::ControlFlowGraph& cfg,
      InstructionAnalyzer<BackwardTaintEnvironment> instruction_analyzer);

  ~BackwardTaintFixpoint();

  void analyze_node(const NodeId& block, BackwardTaintEnvironment* taint)
      const override;

  BackwardTaintEnvironment analyze_edge(
      const EdgeId& edge,
      const BackwardTaintEnvironment& taint) const override;

 private:
  InstructionAnalyzer<BackwardTaintEnvironment> instruction_analyzer_;
};

} // namespace marianatrench

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

#include <mariana-trench/ForwardTaintEnvironment.h>

namespace marianatrench {

class ForwardTaintFixpoint final : public sparta::MonotonicFixpointIterator<
                                       cfg::GraphInterface,
                                       ForwardTaintEnvironment> {
 public:
  ForwardTaintFixpoint(
      const cfg::ControlFlowGraph& cfg,
      InstructionAnalyzer<ForwardTaintEnvironment> instruction_analyzer);

  ~ForwardTaintFixpoint();

  void analyze_node(const NodeId& block, ForwardTaintEnvironment* taint)
      const override;

  ForwardTaintEnvironment analyze_edge(
      const EdgeId& edge,
      const ForwardTaintEnvironment& taint) const override;

 private:
  InstructionAnalyzer<ForwardTaintEnvironment> instruction_analyzer_;
};

} // namespace marianatrench

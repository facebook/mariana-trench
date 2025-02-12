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

#include <mariana-trench/ForwardAliasEnvironment.h>
#include <mariana-trench/MethodContext.h>
#include <mariana-trench/Timer.h>

namespace marianatrench {

class ForwardAliasFixpoint final : public sparta::MonotonicFixpointIterator<
                                       cfg::GraphInterface,
                                       ForwardAliasEnvironment> {
 public:
  ForwardAliasFixpoint(
      MethodContext& context,
      const cfg::ControlFlowGraph& cfg,
      InstructionAnalyzer<ForwardAliasEnvironment> instruction_analyzer);

  ~ForwardAliasFixpoint();

  void analyze_node(const NodeId& block, ForwardAliasEnvironment* environment)
      const override;

  ForwardAliasEnvironment analyze_edge(
      const EdgeId& edge,
      const ForwardAliasEnvironment& environment) const override;

  const Timer& timer() const {
    return timer_;
  }

 private:
  MethodContext& context_;
  InstructionAnalyzer<ForwardAliasEnvironment> instruction_analyzer_;

  // Timer tracking approximately when the analysis was started.
  // Default constructed.
  Timer timer_;
};

} // namespace marianatrench

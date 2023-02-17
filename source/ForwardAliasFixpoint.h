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

#include <mariana-trench/ForwardAliasEnvironment.h>
#include <mariana-trench/MethodContext.h>

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

 private:
  MethodContext& context_;
  InstructionAnalyzer<ForwardAliasEnvironment> instruction_analyzer_;
};

} // namespace marianatrench

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

#include <mariana-trench/ForwardAnalysisEnvironment.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

class ForwardFixpoint final : public sparta::MonotonicFixpointIterator<
                                  cfg::GraphInterface,
                                  ForwardAnalysisEnvironment> {
 public:
  ForwardFixpoint(
      const cfg::ControlFlowGraph& cfg,
      InstructionAnalyzer<ForwardAnalysisEnvironment> instruction_analyzer)
      : MonotonicFixpointIterator(cfg),
        instruction_analyzer_(instruction_analyzer) {}

  virtual ~ForwardFixpoint() {}

  void analyze_node(const NodeId& block, ForwardAnalysisEnvironment* taint)
      const override {
    LOG(4, "Analyzing block {}\n{}", block->id(), *taint);
    for (auto& instruction : *block) {
      switch (instruction.type) {
        case MFLOW_OPCODE:
          instruction_analyzer_(instruction.insn, taint);
          break;
        case MFLOW_POSITION:
          taint->set_last_position(instruction.pos.get());
          break;
        default:
          break;
      }
    }
  }

  ForwardAnalysisEnvironment analyze_edge(
      const EdgeId& /*edge*/,
      const ForwardAnalysisEnvironment& taint) const override {
    return taint;
  }

 private:
  InstructionAnalyzer<ForwardAnalysisEnvironment> instruction_analyzer_;
};

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/local_flow/LocalFlowConstraint.h>
#include <mariana-trench/local_flow/LocalFlowNode.h>
#include <mariana-trench/local_flow/LocalFlowResult.h>

namespace marianatrench {

class Positions;
class CallGraph;

namespace local_flow {

/**
 * Per-method orchestrator for local flow analysis.
 *
 * Steps:
 * 1. Initialize register env with Param/This/Result/Ctrl roots
 * 2. Single-pass CFG walk -> raw LocalFlowConstraints
 * 3. Record exit state (result, this_out)
 * 4. Run temp elimination (bounded closure)
 * 5. Return LocalFlowResult
 */
class LocalFlowMethodAnalyzer {
 public:
  /**
   * Analyze a single method and produce its local flow result.
   *
   * @param method The method to analyze
   * @param max_structural_depth Maximum structural depth for temp elimination
   * @param positions Source positions for call-site annotation (may be nullptr)
   * @param call_graph Call graph for precise callee resolution (may be nullptr)
   * @return The method's local flow result, or std::nullopt if the method
   *         has no code (abstract/native)
   */
  static std::optional<LocalFlowMethodResult> analyze(
      const Method* method,
      std::size_t max_structural_depth,
      const Positions* MT_NULLABLE positions = nullptr,
      const CallGraph* MT_NULLABLE call_graph = nullptr);
};

} // namespace local_flow
} // namespace marianatrench

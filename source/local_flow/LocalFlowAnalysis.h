/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>

#include <mariana-trench/Context.h>
#include <mariana-trench/local_flow/LocalFlowRegistry.h>

namespace marianatrench {
namespace local_flow {

/**
 * Top-level entry point for local flow analysis.
 *
 * Runs independently of the taint fixpoint analysis.
 * Dispatches method analysis in parallel via sparta::WorkQueue.
 */
class LocalFlowAnalysis {
 public:
  /**
   * Run local flow analysis on all methods in the context.
   * Results are stored in the provided registry.
   *
   * @param context The MT global context (provides Methods, ClassHierarchies,
   *   etc.)
   * @param registry Output registry to populate with results
   * @param max_structural_depth Maximum structural depth for temp elimination
   * @param sequential If true, run sequentially (for debugging)
   */
  static void run(
      const Context& context,
      LocalFlowRegistry& registry,
      std::size_t max_structural_depth = 5,
      bool sequential = false);

  /**
   * Summary of an external method without code, which lets a search continue
   * through framework and library calls. Returns std::nullopt for methods with
   * code, non-external methods, and stubs without any flow.
   */
  static std::optional<LocalFlowMethodResult> external_stub(
      const Method* method);
};

} // namespace local_flow
} // namespace marianatrench

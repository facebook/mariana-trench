/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>
#include <string>

#include <json/json.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/local_flow/LocalFlowRegistry.h>

namespace marianatrench {
namespace local_flow {

/**
 * Version of the local flow output contract, written to the bundle metadata
 * as `local_flow_format_version`. Bundles without the field are version 1.
 * Increment it whenever the meaning of emitted ports, labels, guards or node
 * keys changes.
 *
 * Version 2 numbers call arguments like the callee's parameters (receiver p0),
 * emits producer guards, enters static callees and reads constants without a
 * member or source label, keys constructor, private and super calls to the
 * declaring class, labels override edges with an `Interval` call label, uses
 * plain call labels for function-value invokes and the callee node for
 * recursive calls, exports writes to the receiver through this_out, and stubs
 * external methods with MT's default propagation.
 */
constexpr int k_local_flow_format_version = 2;

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

  /**
   * Contents of `local_flow-metadata.json`, through which consumers discover
   * the bundle and its format version.
   */
  static Json::Value bundle_metadata(
      const std::optional<std::string>& commit_hash);
};

} // namespace local_flow
} // namespace marianatrench

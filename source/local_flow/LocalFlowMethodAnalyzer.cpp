/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/local_flow/LocalFlowMethodAnalyzer.h>

namespace marianatrench {
namespace local_flow {

std::optional<LocalFlowMethodResult> LocalFlowMethodAnalyzer::analyze(
    const Method* /* method */,
    std::size_t /* max_structural_depth */,
    const Positions* /* positions */,
    const CallGraph* /* call_graph */) {
  // Stub: returns empty result. Will be replaced with real implementation.
  return std::nullopt;
}

} // namespace local_flow
} // namespace marianatrench

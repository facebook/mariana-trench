/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/local_flow/LocalFlowClassAnalyzer.h>

namespace marianatrench {
namespace local_flow {

std::vector<LocalFlowClassResult> LocalFlowClassAnalyzer::analyze_classes(
    const Context& /* context */,
    const std::unordered_set<const DexType*>* /* relevant_types */) {
  // Stub: returns empty results. Will be replaced with real implementation.
  return {};
}

} // namespace local_flow
} // namespace marianatrench

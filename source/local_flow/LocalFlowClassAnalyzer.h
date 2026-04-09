/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_set>
#include <vector>

#include <mariana-trench/Context.h>
#include <mariana-trench/local_flow/LocalFlowResult.h>

class DexType;

namespace marianatrench {
namespace local_flow {

/**
 * Generates class-level dispatch edges for local flow analysis.
 *
 * For each class with concrete instance methods, generates:
 * 1. Meth -> CVar edges (one per declared method)
 * 2. CVar -> OVar edge (exact dispatch subsumes override dispatch)
 * 3. OVar -> OVar edges with Interval labels for supertypes/interfaces
 *
 * When relevant_types is provided (non-null), only classes whose type is
 * in the set are processed. This is demand-driven: only classes whose
 * methods are actually referenced during the method-level analysis get
 * dispatch edges.
 */
class LocalFlowClassAnalyzer {
 public:
  static std::vector<LocalFlowClassResult> analyze_classes(
      const Context& context,
      const std::unordered_set<const DexType*>* relevant_types = nullptr);
};

} // namespace local_flow
} // namespace marianatrench

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
 * Uses a two-phase parent-centric generation model:
 *
 * Phase 1 (parallel): For each class, generates self edges stored in
 *   the class's own entry:
 *   1. Meth -> CVar edges (one per declared method)
 *   2. CVar -> OVar edge (exact dispatch subsumes override dispatch)
 *   Override hierarchy edges (OVar -> OVar) are collected separately.
 *
 * Phase 2 (sequential): Redistributes override hierarchy edges into
 *   the parent's entry. Each O{Child} -> O{Parent} edge is stored in
 *   Parent's Class entry, enabling the LFE to load O{Parent} and
 *   discover all subclass edges for virtual dispatch traversal.
 *
 * When relevant_types is provided (non-null), only classes whose type is
 * in the set are processed. Parent types not in the set may still get
 * stub entries if they receive override edges from processed children.
 */
class LocalFlowClassAnalyzer {
 public:
  static std::vector<LocalFlowClassResult> analyze_classes(
      const Context& context,
      const std::unordered_set<const DexType*>* relevant_types = nullptr);
};

} // namespace local_flow
} // namespace marianatrench

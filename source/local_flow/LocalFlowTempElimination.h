/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstddef>

#include <mariana-trench/local_flow/LocalFlowConstraint.h>

namespace marianatrench {
namespace local_flow {

/**
 * Bounded closure / temp elimination for local flow constraints.
 *
 * Algorithm:
 * 1. Build adjacency maps for forward and backward edges
 * 2. Seed worklist with self-loops on all global (non-temp) nodes
 * 3. Compose through temp nodes, bounded by max_structural_depth
 * 4. Normalize dual edges
 * 5. Deduplicate final constraint set
 *
 * After elimination, only constraints between non-temp nodes remain.
 */
class LocalFlowTempElimination {
 public:
  /**
   * Eliminate temporary nodes from the constraint set via bounded closure.
   *
   * @param constraints The raw constraint set (modified in place)
   * @param max_structural_depth Maximum structural label depth for composed
   *   edges (default 5, matching Hack)
   * @return true if elimination completed, false if it bailed out due to
   *   exceeding the composed-edge limit (constraints are cleared in that case)
   */
  static bool eliminate(
      LocalFlowConstraintSet& constraints,
      std::size_t max_structural_depth = 5);
};

} // namespace local_flow
} // namespace marianatrench

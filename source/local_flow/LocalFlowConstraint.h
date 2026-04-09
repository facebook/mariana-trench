/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <vector>

#include <json/json.h>

#include <mariana-trench/local_flow/LocalFlowLabel.h>
#include <mariana-trench/local_flow/LocalFlowNode.h>

namespace marianatrench {
namespace local_flow {

/**
 * A raw constraint representing a labeled edge in the local flow graph.
 *
 * Uses a single unified label path where structural and call labels are
 * interleaved in a single vector.
 */
struct LocalFlowConstraint {
  LocalFlowNode source;
  LabelPath labels;
  LocalFlowNode target;
  Guard guard = Guard::Fwd;

  bool operator==(const LocalFlowConstraint& other) const;
  bool operator<(const LocalFlowConstraint& other) const;

  /**
   * Returns the structural label depth (counts only structural-kind labels).
   */
  std::size_t structural_depth() const;

  Json::Value to_json() const;
};

/**
 * Accumulator for local flow constraints with add/filter/iterate operations.
 */
class LocalFlowConstraintSet {
 public:
  LocalFlowConstraintSet() = default;

  void add(LocalFlowConstraint constraint);

  /**
   * Add a simple edge with no labels.
   */
  void add_edge(LocalFlowNode source, LocalFlowNode target);

  /**
   * Add an edge with a unified label path.
   */
  void add_edge(LocalFlowNode source, LabelPath labels, LocalFlowNode target);

  const std::vector<LocalFlowConstraint>& constraints() const;
  std::size_t size() const;
  bool empty() const;

  /**
   * Remove all constraints involving only temporary nodes (both source and
   * target are temps). Used after temp elimination.
   */
  void remove_temp_only();

  /**
   * Deduplicate constraints.
   */
  void deduplicate();

  Json::Value to_json() const;

 private:
  std::vector<LocalFlowConstraint> constraints_;
};

} // namespace local_flow
} // namespace marianatrench

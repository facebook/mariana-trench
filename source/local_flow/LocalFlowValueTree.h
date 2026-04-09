/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <map>
#include <string>
#include <vector>

#include <mariana-trench/local_flow/LocalFlowConstraint.h>
#include <mariana-trench/local_flow/LocalFlowNode.h>

namespace marianatrench {
namespace local_flow {

/**
 * Tree-structured symbolic value domain for local flow analysis.
 *
 * Rooted at a LocalFlowNode, children keyed by field/index names.
 * NOT a SPARTA AbstractDomain -- concrete tree operations only.
 *
 * Supports:
 * - Strong subtree update (write to a field)
 * - Select (read from a field, creating Project label)
 * - Join (merge two trees at a control-flow join point)
 * - Constraint emission (emit all edges from tree structure)
 */
class LocalFlowValueTree {
 public:
  explicit LocalFlowValueTree(LocalFlowNode root);

  const LocalFlowNode& root() const;

  /**
   * Select a child subtree by field name (Project operation).
   * If the child exists, returns a tree rooted at the child's root.
   * If not, creates a fresh temp node for the result.
   */
  LocalFlowValueTree select(const std::string& field, TempIdGenerator& temp_gen)
      const;

  /**
   * Strong update: set a child field to a new subtree.
   * Returns a new tree with the field updated.
   */
  LocalFlowValueTree update(const std::string& field, LocalFlowValueTree child)
      const;

  /**
   * Join two value trees. Used at control-flow merge points.
   * Creates a fresh temp node as the new root, and emits
   * constraints from both trees' roots to the join temp.
   */
  static LocalFlowValueTree join(
      const LocalFlowValueTree& left,
      const LocalFlowValueTree& right,
      TempIdGenerator& temp_gen,
      LocalFlowConstraintSet& constraints);

  /**
   * Flat N-way join. Creates a single join temp with direct fan-in
   * from all input trees, avoiding the O(N²) linear temp chain that
   * pairwise join produces. Used at CFG merge points with many
   * predecessors (e.g., giant switch statements).
   */
  static LocalFlowValueTree join_all(
      const std::vector<const LocalFlowValueTree*>& trees,
      TempIdGenerator& temp_gen,
      LocalFlowConstraintSet& constraints);

  /**
   * Emit constraints that capture the tree structure.
   * For each child at field `f`, emits:
   *   child_root -[Inject:f]-> this_root
   */
  void emit_structure_constraints(LocalFlowConstraintSet& constraints) const;

  /**
   * Check if this tree has any children.
   */
  bool has_children() const;

  /**
   * Get the children map.
   */
  const std::map<std::string, LocalFlowValueTree>& children() const;

 private:
  LocalFlowNode root_;
  std::map<std::string, LocalFlowValueTree> children_;
};

} // namespace local_flow
} // namespace marianatrench

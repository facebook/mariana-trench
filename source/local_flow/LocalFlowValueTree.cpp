/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/local_flow/LocalFlowValueTree.h>

#include <set>

namespace marianatrench {
namespace local_flow {

LocalFlowValueTree::LocalFlowValueTree(LocalFlowNode root)
    : root_(std::move(root)) {}

const LocalFlowNode& LocalFlowValueTree::root() const {
  return root_;
}

LocalFlowValueTree LocalFlowValueTree::select(
    const std::string& field,
    TempIdGenerator& temp_gen) const {
  auto it = children_.find(field);
  if (it != children_.end()) {
    return it->second;
  }
  // No child at this field -- create a fresh temp representing
  // the projection result. The constraint connecting this temp
  // to the parent is emitted by the caller (transfer function).
  return LocalFlowValueTree(LocalFlowNode::make_temp(temp_gen.next()));
}

LocalFlowValueTree LocalFlowValueTree::update(
    const std::string& field,
    LocalFlowValueTree child) const {
  LocalFlowValueTree result(root_);
  result.children_ = children_;
  result.children_.insert_or_assign(field, std::move(child));
  return result;
}

LocalFlowValueTree LocalFlowValueTree::join(
    const LocalFlowValueTree& left,
    const LocalFlowValueTree& right,
    TempIdGenerator& temp_gen,
    LocalFlowConstraintSet& constraints) {
  // If both sides have the same root, no join needed
  if (left.root_ == right.root_ && left.children_.empty() &&
      right.children_.empty()) {
    return left;
  }

  // Create a fresh join temp
  auto join_node = LocalFlowNode::make_temp(temp_gen.next());

  // Emit constraints: left_root -> join_node, right_root -> join_node
  constraints.add_edge(left.root_, join_node);
  constraints.add_edge(right.root_, join_node);

  LocalFlowValueTree result(join_node);

  // Merge children: for fields present in both, recursively join
  // For fields in only one side, keep as-is
  std::set<std::string> all_fields;
  for (const auto& [field, _] : left.children_) {
    all_fields.insert(field);
  }
  for (const auto& [field, _] : right.children_) {
    all_fields.insert(field);
  }

  for (const auto& field : all_fields) {
    auto left_it = left.children_.find(field);
    auto right_it = right.children_.find(field);

    if (left_it != left.children_.end() && right_it != right.children_.end()) {
      // Both sides have this field - recurse
      result.children_.emplace(
          field,
          join(left_it->second, right_it->second, temp_gen, constraints));
    } else if (left_it != left.children_.end()) {
      // Left has field, right doesn't — anchor from right's root.
      // The right side's implicit value at this field is inherited from
      // its root (e.g., if right is `p0`, then right.field is `p0.field`).
      auto right_anchor = LocalFlowNode::make_temp(temp_gen.next());
      constraints.add_edge(
          right.root_,
          LabelPath{Label{LabelKind::Project, field}},
          right_anchor);
      result.children_.emplace(
          field,
          join(
              left_it->second,
              LocalFlowValueTree(right_anchor),
              temp_gen,
              constraints));
    } else {
      // Right has field, left doesn't — anchor from left's root.
      auto left_anchor = LocalFlowNode::make_temp(temp_gen.next());
      constraints.add_edge(
          left.root_, LabelPath{Label{LabelKind::Project, field}}, left_anchor);
      result.children_.emplace(
          field,
          join(
              LocalFlowValueTree(left_anchor),
              right_it->second,
              temp_gen,
              constraints));
    }
  }

  return result;
}

LocalFlowValueTree LocalFlowValueTree::join_all(
    const std::vector<const LocalFlowValueTree*>& trees,
    TempIdGenerator& temp_gen,
    LocalFlowConstraintSet& constraints) {
  if (trees.size() == 1) {
    return *trees[0];
  }

  // Create a single join temp with direct fan-in from all trees.
  // Deduplicate roots so identical roots don't produce redundant edges.
  auto join_node = LocalFlowNode::make_temp(temp_gen.next());
  std::set<LocalFlowNode> seen_roots;
  for (const auto* tree : trees) {
    if (seen_roots.insert(tree->root()).second) {
      constraints.add_edge(tree->root(), join_node);
    }
  }

  LocalFlowValueTree result(join_node);

  // Merge children: collect all fields across all trees
  std::set<std::string> all_fields;
  for (const auto* tree : trees) {
    for (const auto& [field, _] : tree->children_) {
      all_fields.insert(field);
    }
  }

  for (const auto& field : all_fields) {
    std::vector<const LocalFlowValueTree*> field_trees;
    // For trees that DON'T have this field, create anchor temps.
    // Tree union anchor mechanism: the implicit value at a missing field is
    // inherited from the tree's root via Project.
    std::vector<LocalFlowValueTree> anchor_trees; // storage for anchors
    anchor_trees.reserve(
        trees.size()); // prevent reallocation invalidating ptrs
    for (const auto* tree : trees) {
      auto it = tree->children_.find(field);
      if (it != tree->children_.end()) {
        field_trees.push_back(&it->second);
      } else {
        // Anchor: project field from this tree's root
        auto anchor_node = LocalFlowNode::make_temp(temp_gen.next());
        constraints.add_edge(
            tree->root(),
            LabelPath{Label{LabelKind::Project, field}},
            anchor_node);
        anchor_trees.emplace_back(anchor_node);
        field_trees.push_back(&anchor_trees.back());
      }
    }
    if (field_trees.size() == 1) {
      result.children_.emplace(field, *field_trees[0]);
    } else {
      result.children_.emplace(
          field, join_all(field_trees, temp_gen, constraints));
    }
  }

  return result;
}

void LocalFlowValueTree::emit_structure_constraints(
    LocalFlowConstraintSet& constraints) const {
  for (const auto& [field, child] : children_) {
    // child_root -[Inject:field]-> this_root
    constraints.add_edge(
        child.root_, LabelPath{Label{LabelKind::Inject, field}}, root_);
    // Recursively emit child structure
    child.emit_structure_constraints(constraints);
  }
}

bool LocalFlowValueTree::has_children() const {
  return !children_.empty();
}

const std::map<std::string, LocalFlowValueTree>& LocalFlowValueTree::children()
    const {
  return children_;
}

} // namespace local_flow
} // namespace marianatrench

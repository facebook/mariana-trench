/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/local_flow/LocalFlowValueTree.h>

namespace marianatrench {
namespace local_flow {

class LocalFlowValueTreeTest : public testing::Test {};

TEST_F(LocalFlowValueTreeTest, BasicConstruction) {
  auto node = LocalFlowNode::make_param(0);
  LocalFlowValueTree tree(node);

  EXPECT_EQ(tree.root(), node);
  EXPECT_FALSE(tree.has_children());
}

TEST_F(LocalFlowValueTreeTest, SelectExistingChild) {
  auto root = LocalFlowNode::make_param(0);
  auto child_root = LocalFlowNode::make_temp(0);
  LocalFlowValueTree child(child_root);

  auto tree = LocalFlowValueTree(root).update("name", child);
  EXPECT_TRUE(tree.has_children());

  TempIdGenerator gen;
  auto selected = tree.select("name", gen);
  EXPECT_EQ(selected.root(), child_root);
}

TEST_F(LocalFlowValueTreeTest, SelectMissingChild) {
  auto root = LocalFlowNode::make_param(0);
  LocalFlowValueTree tree(root);

  TempIdGenerator gen;
  auto selected = tree.select("nonexistent", gen);
  // Should get a fresh temp node
  EXPECT_TRUE(selected.root().is_temp());
}

TEST_F(LocalFlowValueTreeTest, StrongUpdate) {
  auto root = LocalFlowNode::make_param(0);
  auto child1 = LocalFlowNode::make_temp(0);
  auto child2 = LocalFlowNode::make_temp(1);

  auto tree = LocalFlowValueTree(root)
                  .update("x", LocalFlowValueTree(child1))
                  .update("y", LocalFlowValueTree(child2));

  EXPECT_TRUE(tree.has_children());
  EXPECT_EQ(tree.children().size(), 2u);

  // Strong update replaces existing child
  auto child3 = LocalFlowNode::make_temp(2);
  auto updated = tree.update("x", LocalFlowValueTree(child3));

  TempIdGenerator gen;
  auto selected = updated.select("x", gen);
  EXPECT_EQ(selected.root(), child3);
}

TEST_F(LocalFlowValueTreeTest, JoinSameRoot) {
  auto root = LocalFlowNode::make_param(0);
  LocalFlowValueTree left(root);
  LocalFlowValueTree right(root);

  TempIdGenerator gen;
  LocalFlowConstraintSet constraints;
  auto joined = LocalFlowValueTree::join(left, right, gen, constraints);

  // Same root, no children -> no join temp needed
  EXPECT_EQ(joined.root(), root);
  EXPECT_TRUE(constraints.empty());
}

TEST_F(LocalFlowValueTreeTest, JoinDifferentRoots) {
  auto left_root = LocalFlowNode::make_param(0);
  auto right_root = LocalFlowNode::make_param(1);
  LocalFlowValueTree left(left_root);
  LocalFlowValueTree right(right_root);

  TempIdGenerator gen;
  LocalFlowConstraintSet constraints;
  auto joined = LocalFlowValueTree::join(left, right, gen, constraints);

  // Should create a fresh join temp
  EXPECT_TRUE(joined.root().is_temp());

  // Should emit two constraints: left_root -> join, right_root -> join
  EXPECT_EQ(constraints.size(), 2u);
}

TEST_F(LocalFlowValueTreeTest, EmitStructureConstraints) {
  auto root = LocalFlowNode::make_param(0);
  auto child1 = LocalFlowNode::make_temp(0);
  auto child2 = LocalFlowNode::make_temp(1);

  auto tree = LocalFlowValueTree(root)
                  .update("x", LocalFlowValueTree(child1))
                  .update("y", LocalFlowValueTree(child2));

  LocalFlowConstraintSet constraints;
  tree.emit_structure_constraints(constraints);

  // Should emit: child1 -[Inject:x]-> root, child2 -[Inject:y]-> root
  EXPECT_EQ(constraints.size(), 2u);
}

} // namespace local_flow
} // namespace marianatrench

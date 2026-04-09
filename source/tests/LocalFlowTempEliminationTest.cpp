/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/local_flow/LocalFlowTempElimination.h>

namespace marianatrench {
namespace local_flow {

class LocalFlowTempEliminationTest : public testing::Test {};

TEST_F(LocalFlowTempEliminationTest, NoTemps) {
  LocalFlowConstraintSet constraints;
  auto p0 = LocalFlowNode::make_param(0);
  auto result = LocalFlowNode::make_result();

  constraints.add_edge(p0, result);

  LocalFlowTempElimination::eliminate(constraints);
  EXPECT_EQ(constraints.size(), 1u);

  auto& c = constraints.constraints()[0];
  EXPECT_EQ(c.source, p0);
  EXPECT_EQ(c.target, result);
}

TEST_F(LocalFlowTempEliminationTest, SingleTempChain) {
  // p0 -> t0 -> result
  LocalFlowConstraintSet constraints;
  auto p0 = LocalFlowNode::make_param(0);
  auto t0 = LocalFlowNode::make_temp(0);
  auto result = LocalFlowNode::make_result();

  constraints.add_edge(p0, t0);
  constraints.add_edge(t0, result);

  LocalFlowTempElimination::eliminate(constraints);

  // Should compose through t0: p0 -> result
  EXPECT_EQ(constraints.size(), 1u);
  auto& c = constraints.constraints()[0];
  EXPECT_EQ(c.source, p0);
  EXPECT_EQ(c.target, result);
}

TEST_F(LocalFlowTempEliminationTest, MultipleTempChain) {
  // p0 -> t0 -> t1 -> result
  LocalFlowConstraintSet constraints;
  auto p0 = LocalFlowNode::make_param(0);
  auto t0 = LocalFlowNode::make_temp(0);
  auto t1 = LocalFlowNode::make_temp(1);
  auto result = LocalFlowNode::make_result();

  constraints.add_edge(p0, t0);
  constraints.add_edge(t0, t1);
  constraints.add_edge(t1, result);

  LocalFlowTempElimination::eliminate(constraints);

  EXPECT_EQ(constraints.size(), 1u);
  auto& c = constraints.constraints()[0];
  EXPECT_EQ(c.source, p0);
  EXPECT_EQ(c.target, result);
}

TEST_F(LocalFlowTempEliminationTest, LabelComposition) {
  // p0 -[Project:x]-> t0 -[Inject:y]-> G{foo}
  LocalFlowConstraintSet constraints;
  auto p0 = LocalFlowNode::make_param(0);
  auto t0 = LocalFlowNode::make_temp(0);
  auto global = LocalFlowNode::make_global("foo");

  constraints.add_edge(p0, LabelPath{Label{LabelKind::Project, "x"}}, t0);
  constraints.add_edge(t0, LabelPath{Label{LabelKind::Inject, "y"}}, global);

  LocalFlowTempElimination::eliminate(constraints);

  EXPECT_EQ(constraints.size(), 1u);
  auto& c = constraints.constraints()[0];
  EXPECT_EQ(c.source, p0);
  EXPECT_EQ(c.target, global);
  EXPECT_EQ(c.labels.size(), 2u);
  EXPECT_EQ(c.labels[0].kind, LabelKind::Project);
  EXPECT_EQ(c.labels[0].value, "x");
  EXPECT_EQ(c.labels[1].kind, LabelKind::Inject);
  EXPECT_EQ(c.labels[1].value, "y");
}

TEST_F(LocalFlowTempEliminationTest, DepthBound) {
  // Create a chain that would exceed depth 2:
  // p0 -[P:a]-> t0 -[P:b]-> t1 -[P:c]-> result
  // At depth 2, only p0-[P:a, P:b]->t1 should be kept, not the full chain
  LocalFlowConstraintSet constraints;
  auto p0 = LocalFlowNode::make_param(0);
  auto t0 = LocalFlowNode::make_temp(0);
  auto t1 = LocalFlowNode::make_temp(1);
  auto result = LocalFlowNode::make_result();

  constraints.add_edge(p0, LabelPath{Label{LabelKind::Project, "a"}}, t0);
  constraints.add_edge(t0, LabelPath{Label{LabelKind::Project, "b"}}, t1);
  constraints.add_edge(t1, LabelPath{Label{LabelKind::Project, "c"}}, result);

  LocalFlowTempElimination::eliminate(constraints, /* max_depth */ 2);

  // The full chain has depth 3, which exceeds max_depth=2
  // With max_depth=2, this composition is blocked.
  EXPECT_EQ(constraints.size(), 0u);
}

TEST_F(LocalFlowTempEliminationTest, BranchingTemps) {
  // p0 -> t0 -> result
  // p1 -> t0
  // Both p0 and p1 should reach result
  LocalFlowConstraintSet constraints;
  auto p0 = LocalFlowNode::make_param(0);
  auto p1 = LocalFlowNode::make_param(1);
  auto t0 = LocalFlowNode::make_temp(0);
  auto result = LocalFlowNode::make_result();

  constraints.add_edge(p0, t0);
  constraints.add_edge(p1, t0);
  constraints.add_edge(t0, result);

  LocalFlowTempElimination::eliminate(constraints);

  EXPECT_EQ(constraints.size(), 2u);
}

TEST_F(LocalFlowTempEliminationTest, EmptyConstraintSet) {
  LocalFlowConstraintSet constraints;
  LocalFlowTempElimination::eliminate(constraints);
  EXPECT_EQ(constraints.size(), 0u);
}

} // namespace local_flow
} // namespace marianatrench

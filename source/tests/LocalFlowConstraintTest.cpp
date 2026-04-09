/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/local_flow/LocalFlowConstraint.h>

namespace marianatrench {
namespace local_flow {

class LocalFlowConstraintTest : public testing::Test {};

TEST_F(LocalFlowConstraintTest, BasicConstraint) {
  auto source = LocalFlowNode::make_param(0);
  auto target = LocalFlowNode::make_result();

  LocalFlowConstraint c{source, {}, target};
  EXPECT_EQ(c.source, source);
  EXPECT_EQ(c.target, target);
  EXPECT_TRUE(c.labels.empty());
  EXPECT_EQ(c.structural_depth(), 0u);
  EXPECT_EQ(c.guard, Guard::Fwd);
}

TEST_F(LocalFlowConstraintTest, ConstraintWithLabels) {
  auto source = LocalFlowNode::make_param(0);
  auto target = LocalFlowNode::make_global("Lcom/Foo;.bar");

  LabelPath labels{Label{LabelKind::Inject, "name"}, Label{LabelKind::Call}};

  LocalFlowConstraint c{source, labels, target};
  EXPECT_EQ(c.structural_depth(), 1u);
  EXPECT_EQ(c.labels.size(), 2u);
}

TEST_F(LocalFlowConstraintTest, ConstraintEquality) {
  auto source = LocalFlowNode::make_param(0);
  auto target = LocalFlowNode::make_result();

  LocalFlowConstraint a{source, {}, target};
  LocalFlowConstraint b{source, {}, target};
  EXPECT_EQ(a, b);

  LocalFlowConstraint c{
      source, LabelPath{Label{LabelKind::Project, "x"}}, target};
  EXPECT_FALSE(a == c);
}

TEST_F(LocalFlowConstraintTest, ConstraintSetAddAndSize) {
  LocalFlowConstraintSet set;
  EXPECT_TRUE(set.empty());
  EXPECT_EQ(set.size(), 0u);

  set.add_edge(LocalFlowNode::make_param(0), LocalFlowNode::make_result());
  EXPECT_FALSE(set.empty());
  EXPECT_EQ(set.size(), 1u);
}

TEST_F(LocalFlowConstraintTest, ConstraintSetAddEdgeVariants) {
  LocalFlowConstraintSet set;

  // Simple edge
  set.add_edge(LocalFlowNode::make_param(0), LocalFlowNode::make_result());

  // Edge with labels
  set.add_edge(
      LocalFlowNode::make_param(1),
      LabelPath{Label{LabelKind::Inject, "field"}},
      LocalFlowNode::make_global("G"));

  // Edge with both structural and call labels
  set.add_edge(
      LocalFlowNode::make_param(2),
      LabelPath{Label{LabelKind::Inject, "p0"}, Label{LabelKind::Call}},
      LocalFlowNode::make_global("callee"));

  EXPECT_EQ(set.size(), 3u);
}

TEST_F(LocalFlowConstraintTest, RemoveTempOnly) {
  LocalFlowConstraintSet set;

  // temp -> temp (should be removed)
  set.add_edge(LocalFlowNode::make_temp(0), LocalFlowNode::make_temp(1));

  // param -> temp (should survive)
  set.add_edge(LocalFlowNode::make_param(0), LocalFlowNode::make_temp(2));

  // temp -> result (should survive)
  set.add_edge(LocalFlowNode::make_temp(3), LocalFlowNode::make_result());

  // param -> result (should survive)
  set.add_edge(LocalFlowNode::make_param(1), LocalFlowNode::make_result());

  EXPECT_EQ(set.size(), 4u);

  set.remove_temp_only();
  EXPECT_EQ(set.size(), 3u);
}

TEST_F(LocalFlowConstraintTest, Deduplicate) {
  LocalFlowConstraintSet set;

  auto p0 = LocalFlowNode::make_param(0);
  auto result = LocalFlowNode::make_result();

  set.add_edge(p0, result);
  set.add_edge(p0, result);
  set.add_edge(p0, result);

  EXPECT_EQ(set.size(), 3u);

  set.deduplicate();
  EXPECT_EQ(set.size(), 1u);
}

TEST_F(LocalFlowConstraintTest, JsonSerialization) {
  auto source = LocalFlowNode::make_param(0);
  auto target = LocalFlowNode::make_result();

  LocalFlowConstraint c{
      source,
      LabelPath{Label{LabelKind::Inject, "name"}, Label{LabelKind::Call}},
      target};

  auto json = c.to_json();
  EXPECT_EQ(json["source"].asString(), "p0");
  EXPECT_EQ(json["target"].asString(), "result");
  EXPECT_EQ(json["labels"].size(), 2u);
  EXPECT_EQ(json["labels"][0].asString(), "Inject:name");
  EXPECT_EQ(json["labels"][1].asString(), "Call");
}

TEST_F(LocalFlowConstraintTest, ConstraintSetJson) {
  LocalFlowConstraintSet set;
  set.add_edge(LocalFlowNode::make_param(0), LocalFlowNode::make_result());
  set.add_edge(LocalFlowNode::make_param(1), LocalFlowNode::make_result());

  auto json = set.to_json();
  EXPECT_EQ(json.size(), 2u);
}

TEST_F(LocalFlowConstraintTest, StructuralDepthCountsOnlyStructural) {
  LocalFlowConstraint c{
      LocalFlowNode::make_param(0),
      LabelPath{
          Label{LabelKind::Inject, "p0"},
          Label{LabelKind::Call, {}, true},
          Label{LabelKind::Project, "result"},
          Label{LabelKind::Return, {}, true}},
      LocalFlowNode::make_result()};

  // Only Inject and Project are structural — Call and Return are not
  EXPECT_EQ(c.structural_depth(), 2u);
}

} // namespace local_flow
} // namespace marianatrench

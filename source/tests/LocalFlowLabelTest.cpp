/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/local_flow/LocalFlowLabel.h>

namespace marianatrench {
namespace local_flow {

class LocalFlowLabelTest : public testing::Test {};

TEST_F(LocalFlowLabelTest, StructuralLabelCreation) {
  auto project = StructuralLabel{StructuralOp::Project, "name"};
  EXPECT_EQ(project.op, StructuralOp::Project);
  EXPECT_EQ(project.field, "name");
}

TEST_F(LocalFlowLabelTest, StructuralLabelEquality) {
  auto a = StructuralLabel{StructuralOp::Project, "x"};
  auto b = StructuralLabel{StructuralOp::Project, "x"};
  auto c = StructuralLabel{StructuralOp::Project, "y"};
  auto d = StructuralLabel{StructuralOp::Inject, "x"};

  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_NE(a, d);
}

TEST_F(LocalFlowLabelTest, StructuralLabelDual) {
  auto project = StructuralLabel{StructuralOp::Project, "f"};
  auto dual = project.dual();
  EXPECT_EQ(dual.op, StructuralOp::ContraProject);
  EXPECT_EQ(dual.field, "f");

  auto inject = StructuralLabel{StructuralOp::Inject, "g"};
  auto inject_dual = inject.dual();
  EXPECT_EQ(inject_dual.op, StructuralOp::ContraInject);
  EXPECT_EQ(inject_dual.field, "g");

  // Double dual is identity
  EXPECT_EQ(project.dual().dual(), project);
  EXPECT_EQ(inject.dual().dual(), inject);
}

TEST_F(LocalFlowLabelTest, StructuralLabelInverse) {
  auto project = StructuralLabel{StructuralOp::Project, "f"};
  auto inv = project.inverse();
  EXPECT_EQ(inv.op, StructuralOp::Inject);
  EXPECT_EQ(inv.field, "f");

  auto contra_project = StructuralLabel{StructuralOp::ContraProject, "g"};
  auto contra_inv = contra_project.inverse();
  EXPECT_EQ(contra_inv.op, StructuralOp::ContraInject);
  EXPECT_EQ(contra_inv.field, "g");
}

TEST_F(LocalFlowLabelTest, StructuralLabelSerialization) {
  auto project = StructuralLabel{StructuralOp::Project, "name"};
  EXPECT_EQ(project.to_string(), "Project:name");

  auto inject_wildcard = StructuralLabel{StructuralOp::Inject, ""};
  EXPECT_EQ(inject_wildcard.to_string(), "Inject:*");

  auto contra_project = StructuralLabel{StructuralOp::ContraProject, "x"};
  EXPECT_EQ(contra_project.to_string(), "ContraProject:x");
}

TEST_F(LocalFlowLabelTest, StructuralLabelOrdering) {
  auto a = StructuralLabel{StructuralOp::Project, "a"};
  auto b = StructuralLabel{StructuralOp::Project, "b"};
  auto c = StructuralLabel{StructuralOp::Inject, "a"};

  EXPECT_TRUE(a < b);
  EXPECT_TRUE(a < c); // Project < Inject in enum order
}

// --- Unified Label tests ---

TEST_F(LocalFlowLabelTest, LabelStructuralCreation) {
  auto project = Label{LabelKind::Project, "name"};
  EXPECT_EQ(project.kind, LabelKind::Project);
  EXPECT_EQ(project.value, "name");
  EXPECT_FALSE(project.preserves_context);
  EXPECT_TRUE(project.call_site.empty());
  EXPECT_TRUE(project.is_structural());
  EXPECT_FALSE(project.is_call());
}

TEST_F(LocalFlowLabelTest, LabelCallCreation) {
  auto call = Label{LabelKind::Call, {}, true, "Foo.java:42"};
  EXPECT_EQ(call.kind, LabelKind::Call);
  EXPECT_TRUE(call.preserves_context);
  EXPECT_EQ(call.call_site, "Foo.java:42");
  EXPECT_FALSE(call.is_structural());
  EXPECT_TRUE(call.is_call());
}

TEST_F(LocalFlowLabelTest, LabelEquality) {
  auto a = Label{LabelKind::Project, "x"};
  auto b = Label{LabelKind::Project, "x"};
  auto c = Label{LabelKind::Project, "y"};
  auto d = Label{LabelKind::Inject, "x"};

  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_NE(a, d);

  auto call1 = Label{LabelKind::Call, {}, false, "site1"};
  auto call2 = Label{LabelKind::Call, {}, false, "site1"};
  auto call3 = Label{LabelKind::Call, {}, true, "site1"};
  EXPECT_EQ(call1, call2);
  EXPECT_NE(call1, call3);
}

TEST_F(LocalFlowLabelTest, LabelInverse) {
  // Structural inverses
  EXPECT_EQ(
      Label(LabelKind::Project, "f").inverse(),
      (Label{LabelKind::Inject, "f"}));
  EXPECT_EQ(
      Label(LabelKind::Inject, "f").inverse(),
      (Label{LabelKind::Project, "f"}));
  EXPECT_EQ(
      Label(LabelKind::ContraProject, "f").inverse(),
      (Label{LabelKind::ContraInject, "f"}));
  EXPECT_EQ(
      Label(LabelKind::ContraInject, "f").inverse(),
      (Label{LabelKind::ContraProject, "f"}));

  // Call inverses
  auto call = Label{LabelKind::Call, {}, true, "site"};
  auto ret = call.inverse();
  EXPECT_EQ(ret.kind, LabelKind::Return);
  EXPECT_TRUE(ret.preserves_context);
  EXPECT_EQ(ret.call_site, "site");

  EXPECT_EQ(Label(LabelKind::HoCall).inverse().kind, LabelKind::HoReturn);

  // Capture is self-inverse
  auto capture = Label{LabelKind::Capture};
  EXPECT_EQ(capture.inverse(), capture);
}

TEST_F(LocalFlowLabelTest, LabelSerialization) {
  EXPECT_EQ(Label(LabelKind::Project, "name").to_string(), "Project:name");
  EXPECT_EQ(Label(LabelKind::Inject, "").to_string(), "Inject:*");
  EXPECT_EQ(
      Label(LabelKind::ContraProject, "x").to_string(), "ContraProject:x");
  EXPECT_EQ(Label(LabelKind::Call, {}, false).to_string(), "Call");
  EXPECT_EQ(Label(LabelKind::Call, {}, true).to_string(), "Call+");
  EXPECT_EQ(
      Label(LabelKind::Return, {}, false, "F.java:1").to_string(),
      "Return:F.java:1");
  EXPECT_EQ(Label(LabelKind::HoReturn, {}, true).to_string(), "HoReturn+");
  EXPECT_EQ(Label(LabelKind::Capture).to_string(), "Capture");
  EXPECT_EQ(Label(LabelKind::HoCapture).to_string(), "HoCapture");
  EXPECT_EQ(Label(LabelKind::Interval, "[1,5]").to_string(), "Interval:[1,5]");
}

TEST_F(LocalFlowLabelTest, LabelConversionFromStructural) {
  auto sl = StructuralLabel{StructuralOp::Project, "field"};
  auto label = Label{sl};
  EXPECT_EQ(label.kind, LabelKind::Project);
  EXPECT_EQ(label.value, "field");
  EXPECT_TRUE(label.is_structural());

  auto sl2 = StructuralLabel{StructuralOp::ContraInject, "x"};
  auto label2 = Label{sl2};
  EXPECT_EQ(label2.kind, LabelKind::ContraInject);
  EXPECT_EQ(label2.value, "x");
}

TEST_F(LocalFlowLabelTest, LabelOrdering) {
  // Structural kinds come before call kinds in LabelKind enum
  auto project = Label{LabelKind::Project, "a"};
  auto call = Label{LabelKind::Call};
  EXPECT_TRUE(project < call);

  // Same kind, different value
  auto pa = Label{LabelKind::Project, "a"};
  auto pb = Label{LabelKind::Project, "b"};
  EXPECT_TRUE(pa < pb);
}

// --- Guard / is_contra_path / reverse_guard tests ---

TEST_F(LocalFlowLabelTest, GuardFlip) {
  EXPECT_EQ(flip_guard(Guard::Fwd), Guard::Bwd);
  EXPECT_EQ(flip_guard(Guard::Bwd), Guard::Fwd);
}

TEST_F(LocalFlowLabelTest, IsContraPath) {
  // Empty path: not contra
  EXPECT_FALSE(is_contra_path(LabelPath{}));

  // No contra labels: not contra
  LabelPath no_contra{
      Label{LabelKind::Project, "x"},
      Label{LabelKind::Inject, "y"},
  };
  EXPECT_FALSE(is_contra_path(no_contra));

  // One contra label: is contra (odd count)
  LabelPath one_contra{
      Label{LabelKind::ContraProject, "x"},
      Label{LabelKind::Inject, "y"},
  };
  EXPECT_TRUE(is_contra_path(one_contra));

  // Two contra labels: not contra (even count)
  LabelPath two_contra{
      Label{LabelKind::ContraProject, "x"},
      Label{LabelKind::ContraInject, "y"},
  };
  EXPECT_FALSE(is_contra_path(two_contra));

  // Call labels don't count as contra
  LabelPath call_only{
      Label{LabelKind::Call, {}, true},
      Label{LabelKind::Return, {}, true},
  };
  EXPECT_FALSE(is_contra_path(call_only));
}

TEST_F(LocalFlowLabelTest, ReverseGuard) {
  // Non-contra path: flip the guard
  LabelPath non_contra{Label{LabelKind::Project, "x"}};
  EXPECT_EQ(reverse_guard(Guard::Fwd, non_contra), Guard::Bwd);
  EXPECT_EQ(reverse_guard(Guard::Bwd, non_contra), Guard::Fwd);

  // Contra path: preserve the guard
  LabelPath contra{Label{LabelKind::ContraProject, "x"}};
  EXPECT_EQ(reverse_guard(Guard::Fwd, contra), Guard::Fwd);
  EXPECT_EQ(reverse_guard(Guard::Bwd, contra), Guard::Bwd);

  // Empty path: flip (no contra labels)
  EXPECT_EQ(reverse_guard(Guard::Fwd, LabelPath{}), Guard::Bwd);
}

} // namespace local_flow
} // namespace marianatrench

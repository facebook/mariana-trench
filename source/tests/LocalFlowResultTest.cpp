/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/local_flow/LocalFlowResult.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {
namespace local_flow {

class LocalFlowResultTest : public testing::Test {};

TEST_F(LocalFlowResultTest, SerializesLabelsAndEndpointsByNodeKind) {
  LocalFlowConstraintSet constraints;
  constraints.add_edge(
      LocalFlowNode::make_param(0),
      LabelPath{
          Label{LabelKind::Call, {}, false, "(1,0)"},
          Label{LabelKind::ContraInject, "p0"}},
      LocalFlowNode::make_global("LA;||g:(Ljava/lang/Object;)V"));
  constraints.add_edge(
      LocalFlowNode::make_param(0),
      LabelPath{
          Label{LabelKind::Call, {}, false, "(2,0)"},
          Label{LabelKind::ContraInject, "p1"}},
      LocalFlowNode::make_meth("LB;||m:(Ljava/lang/Object;)V"));
  constraints.add_edge(
      LocalFlowNode::make_static_field("LA;||f"), LocalFlowNode::make_result());
  constraints.add_edge(
      LocalFlowNode::make_param(0), LocalFlowNode::make_static_field("LA;||f"));
  constraints.add_edge(
      LocalFlowNode::make_constant("0"), LocalFlowNode::make_result());

  auto json = LocalFlowMethodResult(
                  "LA;||m:(Ljava/lang/Object;)Ljava/lang/Object;",
                  std::move(constraints),
                  /* relevant_types */ {},
                  /* path */ {},
                  /* callable_line */ 0,
                  /* is_static */ true)
                  .to_json();

  auto expected = test::parse_json(R"#({"localflow": {"flows": [
    {
      "guard": "Bwd",
      "left": "G{LA;||m:(Ljava/lang/Object;)Ljava/lang/Object;}",
      "right": "G{LA;||g:(Ljava/lang/Object;)V}",
      "labels": ["ContraProject:p0", "Call:(1,0)", "ContraInject:p0"]
    },
    {
      "guard": "Bwd",
      "left": "G{LA;||m:(Ljava/lang/Object;)Ljava/lang/Object;}",
      "right": "O{LB;}",
      "labels": [
        "ContraProject:p0",
        "Call:(2,0)",
        "ContraInject:p1",
        "Inject:m:(Ljava/lang/Object;)V"
      ]
    },
    {
      "left": "G{LA;||f}",
      "right": "G{LA;||m:(Ljava/lang/Object;)Ljava/lang/Object;}",
      "labels": ["Project:f", "Inject:result"]
    },
    {
      "guard": "Bwd",
      "left": "G{LA;||m:(Ljava/lang/Object;)Ljava/lang/Object;}",
      "right": "G{LA;||f}",
      "labels": ["ContraProject:p0", "Inject:f"]
    },
    {
      "left": "C<0>",
      "right": "G{LA;||m:(Ljava/lang/Object;)Ljava/lang/Object;}",
      "labels": ["Inject:result"]
    }
  ]}})#");
  EXPECT_EQ(
      test::sorted_local_flow_json(json)["localflow"]["flows"],
      test::sorted_local_flow_json(expected)["localflow"]["flows"]);
}

} // namespace local_flow
} // namespace marianatrench

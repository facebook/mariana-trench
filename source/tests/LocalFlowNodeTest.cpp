/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/local_flow/LocalFlowNode.h>

namespace marianatrench {
namespace local_flow {

class LocalFlowNodeTest : public testing::Test {};

TEST_F(LocalFlowNodeTest, ParamNode) {
  auto p0 = LocalFlowNode::make_param(0);
  auto p1 = LocalFlowNode::make_param(1);

  EXPECT_EQ(p0.kind(), NodeKind::Param);
  EXPECT_FALSE(p0.is_temp());
  EXPECT_TRUE(p0.is_root());
  EXPECT_FALSE(p0.is_global());
  EXPECT_EQ(p0.to_string(), "p0");
  EXPECT_EQ(p1.to_string(), "p1");
  EXPECT_NE(p0, p1);
}

TEST_F(LocalFlowNodeTest, ResultNode) {
  auto result = LocalFlowNode::make_result();
  EXPECT_EQ(result.kind(), NodeKind::Result);
  EXPECT_TRUE(result.is_root());
  EXPECT_EQ(result.to_string(), "result");
}

TEST_F(LocalFlowNodeTest, ThisNode) {
  auto this_node = LocalFlowNode::make_this();
  auto this_out = LocalFlowNode::make_this_out();

  EXPECT_TRUE(this_node.is_root());
  EXPECT_TRUE(this_out.is_root());
  EXPECT_EQ(this_node.to_string(), "this");
  EXPECT_EQ(this_out.to_string(), "this_out");
  EXPECT_NE(this_node, this_out);
}

TEST_F(LocalFlowNodeTest, GlobalNode) {
  auto global = LocalFlowNode::make_global("Lcom/Foo;.bar");
  EXPECT_EQ(global.kind(), NodeKind::Global);
  EXPECT_TRUE(global.is_global());
  EXPECT_FALSE(global.is_temp());
  EXPECT_FALSE(global.is_root());
  EXPECT_EQ(global.to_string(), "G{Lcom/Foo;.bar}");
}

TEST_F(LocalFlowNodeTest, MethNode) {
  auto meth = LocalFlowNode::make_meth("Foo||bar");
  EXPECT_EQ(meth.kind(), NodeKind::Meth);
  EXPECT_TRUE(meth.is_global());
  EXPECT_EQ(meth.to_string(), "M{Foo||bar}");
}

TEST_F(LocalFlowNodeTest, CVarOVarNodes) {
  auto cvar = LocalFlowNode::make_cvar("Foo");
  auto ovar = LocalFlowNode::make_ovar("Foo");

  EXPECT_TRUE(cvar.is_global());
  EXPECT_TRUE(ovar.is_global());
  EXPECT_EQ(cvar.to_string(), "C{Foo}");
  EXPECT_EQ(ovar.to_string(), "O{Foo}");
  EXPECT_NE(cvar, ovar);
}

TEST_F(LocalFlowNodeTest, ConstantNode) {
  auto c = LocalFlowNode::make_constant("42");
  EXPECT_EQ(c.kind(), NodeKind::Constant);
  EXPECT_TRUE(c.is_global());
  EXPECT_EQ(c.to_string(), "C<42>");
}

TEST_F(LocalFlowNodeTest, TempNode) {
  auto t0 = LocalFlowNode::make_temp(0);
  auto t1 = LocalFlowNode::make_temp(1);

  EXPECT_EQ(t0.kind(), NodeKind::Temp);
  EXPECT_TRUE(t0.is_temp());
  EXPECT_FALSE(t0.is_root());
  EXPECT_FALSE(t0.is_global());
  EXPECT_EQ(t0.to_string(), "t0");
  EXPECT_NE(t0, t1);
}

TEST_F(LocalFlowNodeTest, SelfRecNode) {
  auto self_rec = LocalFlowNode::make_self_rec();
  EXPECT_EQ(self_rec.kind(), NodeKind::SelfRec);
  EXPECT_TRUE(self_rec.is_global());
  EXPECT_EQ(self_rec.to_string(), "SelfRec");
}

TEST_F(LocalFlowNodeTest, Equality) {
  auto a = LocalFlowNode::make_param(0);
  auto b = LocalFlowNode::make_param(0);
  auto c = LocalFlowNode::make_result();

  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

TEST_F(LocalFlowNodeTest, Ordering) {
  auto param = LocalFlowNode::make_param(0);
  auto result = LocalFlowNode::make_result();
  auto temp = LocalFlowNode::make_temp(0);

  // Param < Result < ... < Temp (by enum order)
  EXPECT_TRUE(param < result);
  EXPECT_TRUE(result < temp);
}

TEST_F(LocalFlowNodeTest, TempIdGenerator) {
  TempIdGenerator gen;
  EXPECT_EQ(gen.next(), 0u);
  EXPECT_EQ(gen.next(), 1u);
  EXPECT_EQ(gen.next(), 2u);
}

TEST_F(LocalFlowNodeTest, JsonSerialization) {
  auto p0 = LocalFlowNode::make_param(0);
  auto json = p0.to_json();
  EXPECT_EQ(json.asString(), "p0");

  auto global = LocalFlowNode::make_global("Lcom/Foo;.bar");
  json = global.to_json();
  EXPECT_EQ(json.asString(), "G{Lcom/Foo;.bar}");
}

} // namespace local_flow
} // namespace marianatrench

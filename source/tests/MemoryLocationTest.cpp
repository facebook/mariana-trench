/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/MemoryLocation.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class TraceTest : public test::Test {};

TEST_F(TraceTest, MemoryLocationMakeField) {
  auto* left_field = DexString::make_string("left");
  auto* right_field = DexString::make_string("right");

  auto parameter = std::make_unique<ThisParameterMemoryLocation>();
  EXPECT_EQ(parameter->position(), 0);

  auto* parameter_left = parameter->make_field(left_field);
  EXPECT_TRUE(parameter_left != nullptr);
  EXPECT_EQ(parameter_left->field(), left_field);
  EXPECT_EQ(parameter_left->parent(), parameter.get());

  EXPECT_EQ(parameter->make_field(left_field), parameter_left);

  auto* parameter_right = parameter->make_field(right_field);
  EXPECT_TRUE(parameter_right != nullptr);
  EXPECT_TRUE(parameter_right != parameter_left);
  EXPECT_EQ(parameter_right->field(), right_field);
  EXPECT_EQ(parameter_right->parent(), parameter.get());

  auto* parameter_left_right = parameter_left->make_field(right_field);
  EXPECT_TRUE(parameter_left_right != nullptr);
  EXPECT_TRUE(parameter_left_right != parameter_left);
  EXPECT_TRUE(parameter_left_right != parameter_right);
  EXPECT_EQ(parameter_left_right->field(), right_field);
  EXPECT_EQ(parameter_left_right->parent(), parameter_left);

  // Collapse parameter.left.right.left into parameter.left
  auto* parameter_left_right_left =
      parameter_left_right->make_field(left_field);
  EXPECT_TRUE(parameter_left_right_left != nullptr);
  EXPECT_EQ(parameter_left_right_left, parameter_left);
  EXPECT_TRUE(parameter_left_right_left != parameter_right);
  EXPECT_EQ(parameter_left_right_left->field(), left_field);
  EXPECT_EQ(parameter_left_right_left->parent(), parameter.get());
}

TEST_F(TraceTest, MemoryLocationPath) {
  const auto* x = DexString::make_string("x");
  const auto* y = DexString::make_string("y");
  const auto* z = DexString::make_string("z");

  auto parameter = std::make_unique<ParameterMemoryLocation>(1);
  EXPECT_EQ(parameter->root(), parameter.get());
  EXPECT_EQ(parameter->path(), Path{});

  auto this_parameter = std::make_unique<ThisParameterMemoryLocation>();
  EXPECT_EQ(this_parameter->root(), this_parameter.get());
  EXPECT_EQ(this_parameter->path(), Path{});

  auto* parameter_x = parameter->make_field(x);
  EXPECT_EQ(parameter_x->root(), parameter.get());
  EXPECT_EQ(parameter_x->path(), Path{x});

  auto* parameter_x_y = parameter_x->make_field(y);
  EXPECT_EQ(parameter_x_y->root(), parameter.get());
  EXPECT_EQ(parameter_x_y->path(), (Path{x, y}));

  auto* this_parameter_z = this_parameter->make_field(z);
  EXPECT_EQ(this_parameter_z->root(), this_parameter.get());
  EXPECT_EQ(this_parameter_z->path(), Path{z});

  // Collapse parameter.x.y.x into parametre.x
  auto* parameter_x_y_x = parameter_x_y->make_field(x);
  EXPECT_EQ(parameter_x_y_x->root(), parameter.get());
  EXPECT_EQ(parameter_x_y_x->path(), Path{x});
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <IROpcode.h>

#include <mariana-trench/Position.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class PositionTest : public test::Test {};

TEST_F(PositionTest, EqualsOperator) {
  std::string path = "test";
  IRInstruction add_double = IRInstruction(OPCODE_ADD_DOUBLE);
  Position position_one =
      Position(&path, 1, Root(Root::Kind::Return), &add_double);
  Position position_two =
      Position(&path, 1, Root(Root::Kind::Argument, 1), &add_double);

  EXPECT_FALSE(position_one == position_two);
  EXPECT_FALSE(position_one == Position(&path, 1));
  EXPECT_TRUE(
      position_one ==
      Position(&path, 1, Root(Root::Kind::Return), &add_double));
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/constraints/IntegerConstraint.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {
class IntegerConstraintTest : public test::Test {};
} // namespace

TEST_F(IntegerConstraintTest, IntegerConstraintSatisfy) {
  EXPECT_TRUE(IntegerConstraint(1, IntegerConstraint::Operator::EQ).satisfy(1));
  EXPECT_TRUE(IntegerConstraint(1, IntegerConstraint::Operator::NE).satisfy(0));
  EXPECT_TRUE(IntegerConstraint(1, IntegerConstraint::Operator::LE).satisfy(1));
  EXPECT_TRUE(IntegerConstraint(1, IntegerConstraint::Operator::LE).satisfy(0));
  EXPECT_TRUE(IntegerConstraint(1, IntegerConstraint::Operator::LT).satisfy(0));
  EXPECT_TRUE(IntegerConstraint(1, IntegerConstraint::Operator::GE).satisfy(1));
  EXPECT_TRUE(IntegerConstraint(1, IntegerConstraint::Operator::GE).satisfy(2));
  EXPECT_TRUE(IntegerConstraint(1, IntegerConstraint::Operator::GT).satisfy(2));

  EXPECT_FALSE(
      IntegerConstraint(1, IntegerConstraint::Operator::EQ).satisfy(2));
  EXPECT_FALSE(
      IntegerConstraint(1, IntegerConstraint::Operator::NE).satisfy(1));
  EXPECT_FALSE(
      IntegerConstraint(1, IntegerConstraint::Operator::LE).satisfy(2));
  EXPECT_FALSE(
      IntegerConstraint(1, IntegerConstraint::Operator::LT).satisfy(2));
  EXPECT_FALSE(
      IntegerConstraint(1, IntegerConstraint::Operator::GE).satisfy(0));
  EXPECT_FALSE(
      IntegerConstraint(1, IntegerConstraint::Operator::GT).satisfy(0));
}

TEST_F(IntegerConstraintTest, IntegerConstraintFromJson) {
  {
    auto constraint = IntegerConstraint::from_json(test::parse_json(
        R"({
          "constraint": "==",
          "value": 3
        })"));
    EXPECT_EQ(
        IntegerConstraint(3, IntegerConstraint::Operator::EQ), constraint);
  }

  {
    auto constraint = IntegerConstraint::from_json(test::parse_json(
        R"({
          "constraint": ">=",
          "value": 3
        })"));
    EXPECT_EQ(
        IntegerConstraint(3, IntegerConstraint::Operator::GE), constraint);
  }

  {
    auto constraint = IntegerConstraint::from_json(test::parse_json(
        R"({
          "constraint": ">",
          "value": 3
        })"));
    EXPECT_EQ(
        IntegerConstraint(3, IntegerConstraint::Operator::GT), constraint);
  }

  {
    auto constraint = IntegerConstraint::from_json(test::parse_json(
        R"({
          "constraint": "<=",
          "value": 3
        })"));
    EXPECT_EQ(
        IntegerConstraint(3, IntegerConstraint::Operator::LE), constraint);
  }

  {
    auto constraint = IntegerConstraint::from_json(test::parse_json(
        R"({
          "constraint": "<",
          "value": 3
        })"));
    EXPECT_EQ(
        IntegerConstraint(3, IntegerConstraint::Operator::LT), constraint);
  }

  {
    auto constraint = IntegerConstraint::from_json(test::parse_json(
        R"({
          "constraint": "!=",
          "value": 3
        })"));
    EXPECT_EQ(
        IntegerConstraint(3, IntegerConstraint::Operator::NE), constraint);
  }

  EXPECT_THROW(
      IntegerConstraint::from_json(test::parse_json(
          R"({
            "cOnstraint": "==",
            "value": 3
          })")),
      JsonValidationError);

  EXPECT_THROW(
      IntegerConstraint::from_json(test::parse_json(
          R"({
            "constraint": "!==",
            "value": 3
          })")),
      JsonValidationError);

  EXPECT_THROW(
      IntegerConstraint::from_json(test::parse_json(
          R"({
            "constraint": "==",
            "vAlue": 3
          })")),
      JsonValidationError);
}

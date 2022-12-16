/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Creators.h>
#include <gtest/gtest.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/model-generator/JsonModelGenerator.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {
class ParameterConstraintTest : public test::Test {};
} // namespace

TEST_F(ParameterConstraintTest, ParameterConstraintFromJson) {
  // TypeParameterConstraint
  {
    auto constraint = ParameterConstraint::from_json(test::parse_json(
        R"({
            "constraint": "name",
            "pattern": "Landroid/content/Intent;"
           })"));
    EXPECT_EQ(
        TypeParameterConstraint(
            std::make_unique<TypeNameConstraint>("Landroid/content/Intent;")),
        *constraint);
  }

  EXPECT_THROW(
      ParameterConstraint::from_json(test::parse_json(
          R"({
              "constRaint": "name",
              "pattern": "Landroid/content/Intent;"
            })")),
      JsonValidationError);

  EXPECT_THROW(
      ParameterConstraint::from_json(test::parse_json(
          R"({
              "constraint": "name",
              "paAtern": "Landroid/content/Intent;"
            })")),
      JsonValidationError);
  // TypeParameterConstraint
}

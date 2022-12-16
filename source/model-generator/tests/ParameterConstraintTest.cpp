/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Creators.h>
#include <gtest/gtest.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/JsonModelGenerator.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {
class ParameterConstraintTest : public test::Test {};
} // namespace

TEST_F(ParameterConstraintTest, HasAnnotationParameterConstraintSatisfy) {
  std::string class_name = "Landroid/util/Log;";
  auto type = DexType::make_type(DexString::make_string(class_name));
  auto annotations_set =
      redex::create_annotation_set({class_name}, "annotation test");

  EXPECT_TRUE(HasAnnotationParameterConstraint(class_name, {"[a-z ]*"})
                  .satisfy(annotations_set.get(), type));
  EXPECT_TRUE(HasAnnotationParameterConstraint(class_name, {})
                  .satisfy(annotations_set.get(), type));
  EXPECT_FALSE(HasAnnotationParameterConstraint(class_name, {"[A-Z ]*"})
                   .satisfy(annotations_set.get(), type));
  EXPECT_FALSE(HasAnnotationParameterConstraint("Landroid/util/Log", {})
                   .satisfy(annotations_set.get(), type));
}

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

  // HasAnnotationParameterConstraint
  {
    auto constraint = ParameterConstraint::from_json(test::parse_json(
        R"({
            "constraint": "parameter_has_annotation",
            "type": "Lcom/facebook/Annotation;",
            "pattern": "A"
          })"));
    EXPECT_EQ(
        HasAnnotationParameterConstraint("Lcom/facebook/Annotation;", "A"),
        *constraint);
  }

  EXPECT_THROW(
      ParameterConstraint::from_json(test::parse_json(
          R"({
            "constraint": "parameter_has_annotatioN",
            "type": "Lcom/facebook/Annotation;",
            "pattern": "A"
          })")),
      JsonValidationError);

  EXPECT_THROW(
      ParameterConstraint::from_json(test::parse_json(
          R"({
            "constraint": "parameter_has_annotatioN",
            "tyPe": "Lcom/facebook/Annotation;",
            "pattern": "A"
          })")),
      JsonValidationError);

  EXPECT_THROW(
      ParameterConstraint::from_json(test::parse_json(
          R"({
            "constraint": "parameter_has_annotatioN",
            "type": "Lcom/facebook/Annotation;",
            "pattErn": "A"
          })")),
      JsonValidationError);

  EXPECT_THROW(
      ParameterConstraint::from_json(test::parse_json(
          R"({
            "constraint": "parameter_has_annotatioN",
            "type": "Lcom/facebook/Annotation;",
            "Pattern": "A"
          })")),
      JsonValidationError);
  // TypeParameterConstraint
}

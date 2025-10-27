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

TEST_F(ParameterConstraintTest, AllOfParameterConstraintSatisfy) {
  std::string class_name = "Landroid/util/Log;";
  auto type = DexType::make_type(DexString::make_string(class_name));
  auto annotations_set = marianatrench::redex::create_annotation_set(
      {class_name}, "annotation test");

  EXPECT_TRUE(
      AllOfParameterConstraint({}).satisfy(annotations_set.get(), type));

  {
    std::vector<std::unique_ptr<ParameterConstraint>> constraints;
    constraints.push_back(
        std::make_unique<HasAnnotationParameterConstraint>(
            class_name, "[a-z ]*"));

    EXPECT_TRUE(AllOfParameterConstraint(std::move(constraints))
                    .satisfy(annotations_set.get(), type));
  }

  {
    std::vector<std::unique_ptr<ParameterConstraint>> constraints;
    constraints.push_back(
        std::make_unique<HasAnnotationParameterConstraint>(
            class_name, "[a-z ]*"));
    constraints.push_back(
        std::make_unique<HasAnnotationParameterConstraint>(
            class_name, "annotation test"));

    EXPECT_TRUE(AllOfParameterConstraint(std::move(constraints))
                    .satisfy(annotations_set.get(), type));
  }

  {
    std::vector<std::unique_ptr<ParameterConstraint>> constraints;
    constraints.push_back(
        std::make_unique<HasAnnotationParameterConstraint>(
            class_name, "[A-Z ]*"));

    EXPECT_FALSE(AllOfParameterConstraint(std::move(constraints))
                     .satisfy(annotations_set.get(), type));
  }

  {
    std::vector<std::unique_ptr<ParameterConstraint>> constraints;
    constraints.push_back(
        std::make_unique<HasAnnotationParameterConstraint>(
            class_name, "[a-z ]*"));
    constraints.push_back(
        std::make_unique<HasAnnotationParameterConstraint>(
            class_name, "[A-Z ]*"));

    EXPECT_FALSE(AllOfParameterConstraint(std::move(constraints))
                     .satisfy(annotations_set.get(), type));
  }
}

TEST_F(ParameterConstraintTest, AnyOfParameterConstraintSatisfy) {
  std::string class_name = "Landroid/util/Log;";
  auto type = DexType::make_type(DexString::make_string(class_name));
  auto annotations_set = marianatrench::redex::create_annotation_set(
      {class_name}, "annotation test");

  EXPECT_TRUE(
      AnyOfParameterConstraint({}).satisfy(annotations_set.get(), type));

  {
    std::vector<std::unique_ptr<ParameterConstraint>> constraints;
    constraints.push_back(
        std::make_unique<HasAnnotationParameterConstraint>(
            class_name, "[a-z]*"));
    constraints.push_back(
        std::make_unique<TypeParameterConstraint>(
            std::make_unique<TypePatternConstraint>(class_name)));

    EXPECT_TRUE(AnyOfParameterConstraint(std::move(constraints))
                    .satisfy(annotations_set.get(), type));
  }

  {
    std::vector<std::unique_ptr<ParameterConstraint>> constraints;
    constraints.push_back(
        std::make_unique<HasAnnotationParameterConstraint>(
            class_name, "[a-z ]*"));
    constraints.push_back(
        std::make_unique<HasAnnotationParameterConstraint>(
            class_name, "[A-Z]*"));

    EXPECT_TRUE(AnyOfParameterConstraint(std::move(constraints))
                    .satisfy(annotations_set.get(), type));
  }

  {
    std::vector<std::unique_ptr<ParameterConstraint>> constraints;
    constraints.push_back(
        std::make_unique<HasAnnotationParameterConstraint>(
            class_name, "not matched"));
    constraints.push_back(
        std::make_unique<HasAnnotationParameterConstraint>(
            class_name, "[a-z ]*"));
    constraints.push_back(
        std::make_unique<HasAnnotationParameterConstraint>(
            class_name, "[A-Z]*"));

    EXPECT_TRUE(AnyOfParameterConstraint(std::move(constraints))
                    .satisfy(annotations_set.get(), type));
  }

  {
    std::vector<std::unique_ptr<ParameterConstraint>> constraints;
    constraints.push_back(
        std::make_unique<HasAnnotationParameterConstraint>(
            class_name, "[A-Z ]*"));
    constraints.push_back(
        std::make_unique<HasAnnotationParameterConstraint>(
            class_name, "not matched"));
    constraints.push_back(
        std::make_unique<TypeParameterConstraint>(
            std::make_unique<TypePatternConstraint>(
                "Landroid/content/Intent;")));

    EXPECT_FALSE(AnyOfParameterConstraint(std::move(constraints))
                     .satisfy(annotations_set.get(), type));
  }
}

TEST_F(ParameterConstraintTest, NotParameterConstraintSatisfy) {
  std::string class_name = "Landroid/util/Log;";
  auto type = DexType::make_type(DexString::make_string(class_name));
  auto annotations_set = marianatrench::redex::create_annotation_set(
      {class_name}, "annotation test");

  EXPECT_TRUE(NotParameterConstraint(
                  std::make_unique<HasAnnotationParameterConstraint>(
                      class_name, "[A-Z ]*"))
                  .satisfy(annotations_set.get(), type));

  EXPECT_FALSE(NotParameterConstraint(
                   std::make_unique<HasAnnotationParameterConstraint>(
                       class_name, "[a-z ]*"))
                   .satisfy(annotations_set.get(), type));
}

TEST_F(ParameterConstraintTest, HasAnnotationParameterConstraintSatisfy) {
  std::string class_name = "Landroid/util/Log;";
  auto type = DexType::make_type(DexString::make_string(class_name));
  auto annotations_set = marianatrench::redex::create_annotation_set(
      {class_name}, "annotation test");

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
  // AnyOfParameterConstraint
  {
    auto constraint = ParameterConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "any_of",
          "inners": [
            {
              "constraint": "name",
              "pattern": "Landroid/content/Intent;"
            },
            {
              "constraint": "parameter_has_annotation",
              "type": "Lcom/facebook/Annotation;",
              "pattern": "A"
            }
          ]
        })"));
    {
      std::vector<std::unique_ptr<ParameterConstraint>> constraints;
      constraints.push_back(
          std::make_unique<TypeParameterConstraint>(
              std::make_unique<TypePatternConstraint>(
                  "Landroid/content/Intent;")));
      constraints.push_back(
          std::make_unique<HasAnnotationParameterConstraint>(
              "Lcom/facebook/Annotation;", "A"));

      EXPECT_EQ(AnyOfParameterConstraint(std::move(constraints)), *constraint);
    }
    {
      std::vector<std::unique_ptr<ParameterConstraint>> constraints;
      constraints.push_back(
          std::make_unique<HasAnnotationParameterConstraint>(
              "Lcom/facebook/Annotation;", "A"));
      constraints.push_back(
          std::make_unique<TypeParameterConstraint>(
              std::make_unique<TypePatternConstraint>(
                  "Landroid/content/Intent;")));

      EXPECT_EQ(AnyOfParameterConstraint(std::move(constraints)), *constraint);
    }
  }

  EXPECT_THROW(
      ParameterConstraint::from_json(
          test::parse_json(
              R"({
          "constraint": "any_of",
          "inNers": [
            {
              "constraint": "name",
              "pattern": "Landroid/content/Intent;"
            },
            {
              "constraint": "parameter_has_annotation",
              "type": "Lcom/facebook/Annotation;",
              "pattern": "A"
            }
          ]
        })")),
      JsonValidationError);

  EXPECT_THROW(
      ParameterConstraint::from_json(
          test::parse_json(
              R"({
            "Constraint": "any_of",
            "inners": [
              {
                "constraint": "name",
                "pattern": "Landroid/content/Intent;"
              },
              {
                "constraint": "parameter_has_annotation",
                "type": "Lcom/facebook/Annotation;",
                "pattern": "A"
              }
            ]
          })")),
      JsonValidationError);

  EXPECT_THROW(
      ParameterConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "Any_of",
            "inners": [
              {
                "constraint": "name",
                "pattern": "Landroid/content/Intent;"
              },
              {
                "constraint": "parameter_has_annotation",
                "type": "Lcom/facebook/Annotation;",
                "pattern": "A"
              }
            ]
          })")),
      JsonValidationError);
  // AnyOfParameterConstraint

  // AllOfParameterConstraint
  {
    auto constraint = ParameterConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "all_of",
          "inners": [
            {
              "constraint": "name",
              "pattern": "Landroid/content/Intent;"
            },
            {
              "constraint": "parameter_has_annotation",
              "type": "Lcom/facebook/Annotation;",
              "pattern": "A"
            }
          ]
        })"));
    {
      std::vector<std::unique_ptr<ParameterConstraint>> constraints;
      constraints.push_back(
          std::make_unique<TypeParameterConstraint>(
              std::make_unique<TypePatternConstraint>(
                  "Landroid/content/Intent;")));
      constraints.push_back(
          std::make_unique<HasAnnotationParameterConstraint>(
              "Lcom/facebook/Annotation;", "A"));

      EXPECT_EQ(AllOfParameterConstraint(std::move(constraints)), *constraint);
    }
    {
      std::vector<std::unique_ptr<ParameterConstraint>> constraints;
      constraints.push_back(
          std::make_unique<HasAnnotationParameterConstraint>(
              "Lcom/facebook/Annotation;", "A"));
      constraints.push_back(
          std::make_unique<TypeParameterConstraint>(
              std::make_unique<TypePatternConstraint>(
                  "Landroid/content/Intent;")));

      EXPECT_EQ(AllOfParameterConstraint(std::move(constraints)), *constraint);
    }
  }

  EXPECT_THROW(
      ParameterConstraint::from_json(
          test::parse_json(
              R"({
          "constraint": "all_of",
          "inNers": [
            {
              "constraint": "name",
              "pattern": "Landroid/content/Intent;"
            },
            {
              "constraint": "parameter_has_annotation",
              "type": "Lcom/facebook/Annotation;",
              "pattern": "A"
            }
          ]
        })")),
      JsonValidationError);

  EXPECT_THROW(
      ParameterConstraint::from_json(
          test::parse_json(
              R"({
            "Constraint": "all_of",
            "inners": [
              {
                "constraint": "name",
                "pattern": "Landroid/content/Intent;"
              },
              {
                "constraint": "parameter_has_annotation",
                "type": "Lcom/facebook/Annotation;",
                "pattern": "A"
              }
            ]
          })")),
      JsonValidationError);

  EXPECT_THROW(
      ParameterConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "All_of",
            "inners": [
              {
                "constraint": "name",
                "pattern": "Landroid/content/Intent;"
              },
              {
                "constraint": "parameter_has_annotation",
                "type": "Lcom/facebook/Annotation;",
                "pattern": "A"
              }
            ]
          })")),
      JsonValidationError);
  // AllOfParameterConstraint

  // NotParameterConstraint
  {
    auto constraint = ParameterConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "not",
          "inner": {
                "constraint": "parameter_has_annotation",
                "type": "Lcom/facebook/Annotation;",
                "pattern": "A"
              }
        })"));
    EXPECT_EQ(
        NotParameterConstraint(
            std::make_unique<HasAnnotationParameterConstraint>(
                "Lcom/facebook/Annotation;", "A")),
        *constraint);
  }

  EXPECT_THROW(
      ParameterConstraint::from_json(
          test::parse_json(
              R"({
          "cOnstraint": "not",
          "inner": {
                "constraint": "parameter_has_annotation",
                "type": "Lcom/facebook/Annotation;",
                "pattern": "A"
              }
        })")),
      JsonValidationError);

  EXPECT_THROW(
      ParameterConstraint::from_json(
          test::parse_json(
              R"({
          "constraint": "Not",
          "inner": {
                "constraint": "parameter_has_annotation",
                "type": "Lcom/facebook/Annotation;",
                "pattern": "A"
              }
        })")),
      JsonValidationError);

  EXPECT_THROW(
      ParameterConstraint::from_json(
          test::parse_json(
              R"({
          "constraint": "not",
          "iNner": {
                "constraint": "parameter_has_annotation",
                "type": "Lcom/facebook/Annotation;",
                "pattern": "A"
              }
        })")),
      JsonValidationError);
  // NotParameterConstraint

  // TypeParameterConstraint
  {
    auto constraint = ParameterConstraint::from_json(
        test::parse_json(
            R"({
            "constraint": "name",
            "pattern": "Landroid/content/Intent;"
           })"));
    EXPECT_EQ(
        TypeParameterConstraint(
            std::make_unique<TypePatternConstraint>(
                "Landroid/content/Intent;")),
        *constraint);
  }

  EXPECT_THROW(
      ParameterConstraint::from_json(
          test::parse_json(
              R"({
              "constRaint": "name",
              "pattern": "Landroid/content/Intent;"
            })")),
      JsonValidationError);

  EXPECT_THROW(
      ParameterConstraint::from_json(
          test::parse_json(
              R"({
              "constraint": "name",
              "paAtern": "Landroid/content/Intent;"
            })")),
      JsonValidationError);
  // TypeParameterConstraint

  // HasAnnotationParameterConstraint
  {
    auto constraint = ParameterConstraint::from_json(
        test::parse_json(
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
      ParameterConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "parameter_has_annotatioN",
            "type": "Lcom/facebook/Annotation;",
            "pattern": "A"
          })")),
      JsonValidationError);

  EXPECT_THROW(
      ParameterConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "parameter_has_annotatioN",
            "tyPe": "Lcom/facebook/Annotation;",
            "pattern": "A"
          })")),
      JsonValidationError);

  EXPECT_THROW(
      ParameterConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "parameter_has_annotatioN",
            "type": "Lcom/facebook/Annotation;",
            "pattErn": "A"
          })")),
      JsonValidationError);

  EXPECT_THROW(
      ParameterConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "parameter_has_annotatioN",
            "type": "Lcom/facebook/Annotation;",
            "Pattern": "A"
          })")),
      JsonValidationError);
  // TypeParameterConstraint
}

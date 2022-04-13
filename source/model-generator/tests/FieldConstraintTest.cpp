/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/Fields.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/constraints/FieldConstraints.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {
class FieldConstraintTest : public test::Test {};
} // namespace

TEST_F(FieldConstraintTest, FieldNameConstraintSatisfy) {
  std::string field_name = "field_name";
  Scope scope;
  auto dex_field = redex::create_field(
      scope, "LClass;", /* field */ {field_name, type::java_lang_String()});
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto field = context.fields->get(dex_field);

  EXPECT_TRUE(FieldNameConstraint(field_name).satisfy(field));
  EXPECT_FALSE(FieldNameConstraint("LClass;.field_name:Ljava/lang/String;")
                   .satisfy(field));
  EXPECT_TRUE(FieldNameConstraint("([A-Za-z/]*_?)+").satisfy(field));
  EXPECT_FALSE(FieldNameConstraint("([A-Za-z/]*_)+").satisfy(field));
}

TEST_F(FieldConstraintTest, SignatureFieldConstraintSatisfy) {
  Scope scope;
  auto dex_field = redex::create_field(
      scope, "LClass;", /* field */ {"field_name", type::java_lang_String()});
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto field = context.fields->get(dex_field);

  EXPECT_TRUE(
      SignatureFieldConstraint("LClass;\\.field_name\\:Ljava/lang/String;")
          .satisfy(field));
  EXPECT_TRUE(SignatureFieldConstraint(".*field_name.*").satisfy(field));
  EXPECT_FALSE(SignatureFieldConstraint("LClass.field_name").satisfy(field));
  EXPECT_FALSE(SignatureFieldConstraint("field_name").satisfy(field));
}

TEST_F(FieldConstraintTest, HasAnnotationFieldConstraintSatisfy) {
  std::string field_name = "field_name";
  Scope scope;
  auto dex_field = redex::create_field(
      scope,
      "LClass;",
      /* field */
      {field_name,
       type::java_lang_String(),
       /* annotations */
       {"Lcom/facebook/Annotation;"}});
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto field = context.fields->get(dex_field);

  EXPECT_TRUE(
      HasAnnotationFieldConstraint(
          /* type */ "Lcom/facebook/Annotation;", /* annotation */ std::nullopt)
          .satisfy(field));
  EXPECT_FALSE(
      HasAnnotationFieldConstraint(
          /* type */ "Lcom/facebook/Annotation;", /* annotation */ ".*")
          .satisfy(field));
  EXPECT_FALSE(HasAnnotationFieldConstraint(
                   /* type */ "Lcom/facebook/DifferentAnnotation;",
                   /* annotation */ std::nullopt)
                   .satisfy(field));
}

TEST_F(FieldConstraintTest, IsStaticFieldConstraintSatisfy) {
  Scope scope;
  auto dex_field_public = redex::create_field(
      scope, "LClassB;", /* field */ {"field", type::java_lang_String()});
  auto dex_field_static = redex::create_field(
      scope,
      "LClassA;",
      /* field */ {"field", type::java_lang_String()},
      nullptr,
      true);
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto field_public = context.fields->get(dex_field_public);
  auto field_static = context.fields->get(dex_field_static);

  EXPECT_TRUE(IsStaticFieldConstraint(true).satisfy(field_static));
  EXPECT_TRUE(IsStaticFieldConstraint(false).satisfy(field_public));
}

TEST_F(FieldConstraintTest, ParentFieldConstraintSatisfy) {
  Scope scope;
  auto dex_field = redex::create_field(
      scope, "LClass;", /* field */ {"field_name", type::java_lang_String()});
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto field = context.fields->get(dex_field);

  EXPECT_TRUE(
      ParentFieldConstraint(std::make_unique<TypeNameConstraint>("LClass;"))
          .satisfy(field));

  EXPECT_FALSE(ParentFieldConstraint(
                   std::make_unique<TypeNameConstraint>("LDifferentClass;"))
                   .satisfy(field));
}

TEST_F(FieldConstraintTest, AllOfFieldConstraintSatisfy) {
  std::string field_name = "field_name";
  Scope scope;
  auto dex_field = redex::create_field(
      scope, "LClass;", {field_name, type::java_lang_String()});
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto field = context.fields->get(dex_field);

  EXPECT_TRUE(AllOfFieldConstraint({}).satisfy(field));

  {
    std::vector<std::unique_ptr<FieldConstraint>> constraints;
    constraints.push_back(std::make_unique<FieldNameConstraint>(field_name));

    EXPECT_TRUE(AllOfFieldConstraint(std::move(constraints)).satisfy(field));
  }

  {
    std::vector<std::unique_ptr<FieldConstraint>> constraints;
    constraints.push_back(std::make_unique<FieldNameConstraint>(field_name));
    constraints.push_back(std::make_unique<FieldNameConstraint>(".*"));

    EXPECT_TRUE(AllOfFieldConstraint(std::move(constraints)).satisfy(field));
  }

  {
    std::vector<std::unique_ptr<FieldConstraint>> constraints;
    constraints.push_back(
        std::make_unique<FieldNameConstraint>("another_field"));

    EXPECT_FALSE(AllOfFieldConstraint(std::move(constraints)).satisfy(field));
  }

  {
    std::vector<std::unique_ptr<FieldConstraint>> constraints;
    constraints.push_back(std::make_unique<FieldNameConstraint>(".*"));
    constraints.push_back(
        std::make_unique<FieldNameConstraint>("another_field"));

    EXPECT_FALSE(AllOfFieldConstraint(std::move(constraints)).satisfy(field));
  }
}

TEST_F(FieldConstraintTest, AnyOfFieldConstraintSatisfy) {
  std::string field_name = "field_name";
  Scope scope;
  auto dex_field = redex::create_field(
      scope, "LClass;", {field_name, type::java_lang_String()});
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto field = context.fields->get(dex_field);

  EXPECT_TRUE(AnyOfFieldConstraint({}).satisfy(field));

  {
    std::vector<std::unique_ptr<FieldConstraint>> constraints;
    constraints.push_back(std::make_unique<FieldNameConstraint>(field_name));

    EXPECT_TRUE(AnyOfFieldConstraint(std::move(constraints)).satisfy(field));
  }

  {
    std::vector<std::unique_ptr<FieldConstraint>> constraints;
    constraints.push_back(std::make_unique<FieldNameConstraint>(field_name));
    constraints.push_back(std::make_unique<FieldNameConstraint>(".*"));

    EXPECT_TRUE(AnyOfFieldConstraint(std::move(constraints)).satisfy(field));
  }

  {
    std::vector<std::unique_ptr<FieldConstraint>> constraints;
    constraints.push_back(
        std::make_unique<FieldNameConstraint>("another_field"));

    EXPECT_FALSE(AnyOfFieldConstraint(std::move(constraints)).satisfy(field));
  }

  {
    std::vector<std::unique_ptr<FieldConstraint>> constraints;
    constraints.push_back(std::make_unique<FieldNameConstraint>(".*"));
    constraints.push_back(
        std::make_unique<FieldNameConstraint>("another_field"));

    EXPECT_TRUE(AnyOfFieldConstraint(std::move(constraints)).satisfy(field));
  }
}

TEST_F(FieldConstraintTest, FieldNameConstraintFromJson) {
  auto context = test::make_empty_context();

  {
    auto constraint = FieldConstraint::from_json(test::parse_json(
        R"({
          "constraint": "name",
          "pattern": "mfield"
        })"));
    EXPECT_EQ(FieldNameConstraint("mfield"), *constraint);
  }

  EXPECT_THROW(
      FieldConstraint::from_json(test::parse_json(
          R"({
            "cOnstraint": "name",
            "pattern": "println"
          })")),
      JsonValidationError);

  EXPECT_THROW(
      FieldConstraint::from_json(test::parse_json(
          R"({
            "constraint": "nAme",
            "pattern": "println"
          })")),
      JsonValidationError);

  EXPECT_THROW(
      FieldConstraint::from_json(test::parse_json(
          R"({
            "constraint": "name",
            "paTtern": "println"
          })")),
      JsonValidationError);
}

TEST_F(FieldConstraintTest, SignatureFieldConstraintFromJson) {
  auto context = test::make_empty_context();

  {
    auto constraint = FieldConstraint::from_json(test::parse_json(
        R"({
          "constraint": "signature",
          "pattern": "LClass;.mfield:Ltype;"
        })"));
    EXPECT_EQ(SignatureFieldConstraint("LClass;.mfield:Ltype;"), *constraint);
  }

  EXPECT_THROW(
      FieldConstraint::from_json(test::parse_json(
          R"({
            "constraint": "Signature",
            "pattern": "println"
          })")),
      JsonValidationError);
}

TEST_F(FieldConstraintTest, HasAnnotationFieldConstraintFromJson) {
  auto context = test::make_empty_context();

  {
    auto constraint = FieldConstraint::from_json(test::parse_json(
        R"({
          "constraint": "has_annotation",
          "type": "Lcom/facebook/Annotation;",
          "pattern": "A"
        })"));
    EXPECT_EQ(
        HasAnnotationFieldConstraint("Lcom/facebook/Annotation;", "A"),
        *constraint);
  }

  {
    auto constraint = FieldConstraint::from_json(test::parse_json(
        R"({
          "constraint": "has_annotation",
          "type": "Lcom/facebook/Annotation;"
        })"));
    EXPECT_EQ(
        HasAnnotationFieldConstraint("Lcom/facebook/Annotation;", std::nullopt),
        *constraint);
  }

  EXPECT_THROW(
      FieldConstraint::from_json(test::parse_json(
          R"({
            "constraint": "Has_annotation",
            "type": "Lcom/facebook/Annotation;",
            "pattern": "A"
          })")),
      JsonValidationError);
}

TEST_F(FieldConstraintTest, ParentFieldConstraintFromJson) {
  auto context = test::make_empty_context();

  {
    auto constraint = FieldConstraint::from_json(test::parse_json(
        R"({
          "constraint": "parent",
          "inner": {
            "constraint": "name",
            "pattern": "LClass;"
          }
        })"));
    EXPECT_EQ(
        ParentFieldConstraint(std::make_unique<TypeNameConstraint>("LClass;")),
        *constraint);
  }

  EXPECT_THROW(
      FieldConstraint::from_json(test::parse_json(
          R"({
            "constraint": "parent",
            "pattern": "LClass;"
          })")),
      JsonValidationError);
}

TEST_F(FieldConstraintTest, AllOfFieldConstraintFromJson) {
  auto context = test::make_empty_context();

  {
    auto constraint = FieldConstraint::from_json(test::parse_json(
        R"({
          "constraint": "all_of",
          "inners": [
            {
              "constraint": "name",
              "pattern": "println"
            },
            {
              "constraint": "has_annotation",
              "type": "Lcom/facebook/Annotation;"
            }
          ]
        })"));

    {
      std::vector<std::unique_ptr<FieldConstraint>> constraints;
      constraints.push_back(std::make_unique<FieldNameConstraint>("println"));
      constraints.push_back(std::make_unique<HasAnnotationFieldConstraint>(
          "Lcom/facebook/Annotation;", std::nullopt));

      EXPECT_EQ(AllOfFieldConstraint(std::move(constraints)), *constraint);
    }

    {
      std::vector<std::unique_ptr<FieldConstraint>> constraints;
      constraints.push_back(std::make_unique<HasAnnotationFieldConstraint>(
          "Lcom/facebook/Annotation;", std::nullopt));
      constraints.push_back(std::make_unique<FieldNameConstraint>("println"));

      EXPECT_EQ(AllOfFieldConstraint(std::move(constraints)), *constraint);
    }
  }

  EXPECT_THROW(
      FieldConstraint::from_json(test::parse_json(
          R"({
            "constraint": "All_of",
            "inners": [
              {
                "constraint": "name",
                "pattern": "println"
              },
              {
                "constraint": "has_annotation",
              "type": "Lcom/facebook/Annotation;"
              }
            ]
          })")),
      JsonValidationError);
}

TEST_F(FieldConstraintTest, AnyOfFieldConstraintFromJson) {
  auto context = test::make_empty_context();

  {
    auto constraint = FieldConstraint::from_json(test::parse_json(
        R"({
          "constraint": "any_of",
          "inners": [
            {
              "constraint": "name",
              "pattern": "println"
            },
            {
              "constraint": "has_annotation",
              "type": "Lcom/facebook/Annotation;"
            }
          ]
        })"));

    {
      std::vector<std::unique_ptr<FieldConstraint>> constraints;
      constraints.push_back(std::make_unique<FieldNameConstraint>("println"));
      constraints.push_back(std::make_unique<HasAnnotationFieldConstraint>(
          "Lcom/facebook/Annotation;", std::nullopt));

      EXPECT_EQ(AnyOfFieldConstraint(std::move(constraints)), *constraint);
    }

    {
      std::vector<std::unique_ptr<FieldConstraint>> constraints;
      constraints.push_back(std::make_unique<HasAnnotationFieldConstraint>(
          "Lcom/facebook/Annotation;", std::nullopt));
      constraints.push_back(std::make_unique<FieldNameConstraint>("println"));

      EXPECT_EQ(AnyOfFieldConstraint(std::move(constraints)), *constraint);
    }
  }

  EXPECT_THROW(
      FieldConstraint::from_json(test::parse_json(
          R"({
            "constraint": "any_of",
            "inners":
              {
                "constraint": "name",
                "pattern": "println"
              }
          })")),
      JsonValidationError);
}

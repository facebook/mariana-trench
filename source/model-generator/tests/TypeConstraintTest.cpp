/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Creators.h>
#include <gtest/gtest.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/model-generator/TypeConstraints.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {
class TypeConstraintTest : public test::Test {};
} // namespace

TEST_F(TypeConstraintTest, AllOfTypeConstraintSatisfy) {
  std::string class_name = "Landroid/util/Log;";
  auto type = DexType::make_type(DexString::make_string(class_name));

  EXPECT_TRUE(AllOfTypeConstraint({}).satisfy(type));

  {
    std::vector<std::unique_ptr<TypeConstraint>> constraints;
    constraints.push_back(std::make_unique<TypeNameConstraint>(class_name));

    EXPECT_TRUE(AllOfTypeConstraint(std::move(constraints)).satisfy(type));
  }

  {
    std::vector<std::unique_ptr<TypeConstraint>> constraints;
    constraints.push_back(std::make_unique<TypeNameConstraint>(class_name));
    constraints.push_back(std::make_unique<TypeNameConstraint>(".*"));

    EXPECT_TRUE(AllOfTypeConstraint(std::move(constraints)).satisfy(type));
  }

  {
    std::vector<std::unique_ptr<TypeConstraint>> constraints;
    constraints.push_back(
        std::make_unique<TypeNameConstraint>("Landroid/util/Log"));

    EXPECT_FALSE(AllOfTypeConstraint(std::move(constraints)).satisfy(type));
  }

  {
    std::vector<std::unique_ptr<TypeConstraint>> constraints;
    constraints.push_back(std::make_unique<TypeNameConstraint>("Landroid"));
    constraints.push_back(
        std::make_unique<TypeNameConstraint>("Landroid/util/Log"));

    EXPECT_FALSE(AllOfTypeConstraint(std::move(constraints)).satisfy(type));
  }
}

TEST_F(TypeConstraintTest, AnyOfTypeConstraintSatisfy) {
  std::string class_name = "Landroid/util/Log;";
  auto type = DexType::make_type(DexString::make_string(class_name));

  EXPECT_TRUE(AnyOfTypeConstraint({}).satisfy(type));

  {
    std::vector<std::unique_ptr<TypeConstraint>> constraints;
    constraints.push_back(std::make_unique<TypeNameConstraint>(class_name));

    EXPECT_TRUE(AnyOfTypeConstraint(std::move(constraints)).satisfy(type));
  }

  {
    std::vector<std::unique_ptr<TypeConstraint>> constraints;
    constraints.push_back(std::make_unique<TypeNameConstraint>(class_name));
    constraints.push_back(
        std::make_unique<TypeNameConstraint>("Landroid/util/Log"));

    EXPECT_TRUE(AnyOfTypeConstraint(std::move(constraints)).satisfy(type));
  }

  {
    std::vector<std::unique_ptr<TypeConstraint>> constraints;
    constraints.push_back(
        std::make_unique<TypeNameConstraint>("Landroid/util/Log"));

    EXPECT_FALSE(AnyOfTypeConstraint(std::move(constraints)).satisfy(type));
  }

  {
    std::vector<std::unique_ptr<TypeConstraint>> constraints;
    constraints.push_back(std::make_unique<TypeNameConstraint>("Landroid"));
    constraints.push_back(
        std::make_unique<TypeNameConstraint>("Landroid/util/Log"));

    EXPECT_FALSE(AllOfTypeConstraint(std::move(constraints)).satisfy(type));
  }
}

TEST_F(TypeConstraintTest, NotTypeConstraintSatisfy) {
  std::string class_name = "Landroid/util/Log;";
  auto type = DexType::make_type(DexString::make_string(class_name));

  EXPECT_FALSE(
      NotTypeConstraint(std::make_unique<TypeNameConstraint>(class_name))
          .satisfy(type));
  EXPECT_TRUE(NotTypeConstraint(
                  std::make_unique<TypeNameConstraint>("Landroid/util/Log"))
                  .satisfy(type));
}

TEST_F(TypeConstraintTest, IsClassTypeConstraintSatisfy) {
  std::string class_name = "Landroid/util/Log;";
  ClassCreator creator(DexType::make_type(DexString::make_string(class_name)));
  creator.set_super(type::java_lang_Object());
  auto test_class = creator.create();

  EXPECT_TRUE(IsClassTypeConstraint(true).satisfy(test_class->get_type()));
  EXPECT_TRUE(IsClassTypeConstraint().satisfy(test_class->get_type()));
  EXPECT_TRUE(IsClassTypeConstraint(true).satisfy(type::java_lang_Void()));
  EXPECT_FALSE(IsClassTypeConstraint(false).satisfy(type::java_lang_Object()));
  EXPECT_TRUE(IsClassTypeConstraint(false).satisfy(type::_int()));
  EXPECT_FALSE(IsClassTypeConstraint(true).satisfy(type::_boolean()));
}

TEST_F(TypeConstraintTest, IsInterfaceTypeConstraint) {
  std::string interface_name = "Landroid/util/LogInterface";
  ClassCreator interface_creator(
      DexType::make_type(DexString::make_string(interface_name)));
  interface_creator.set_access(DexAccessFlags::ACC_INTERFACE);
  interface_creator.set_super(type::java_lang_Object());
  auto test_interface = interface_creator.create();

  std::string class_name = "Landroid/util/Log;";
  ClassCreator creator(DexType::make_type(DexString::make_string(class_name)));
  creator.set_super(type::java_lang_Object());
  creator.add_interface(
      DexType::make_type(DexString::make_string(interface_name)));
  auto test_class = creator.create();

  EXPECT_TRUE(
      IsInterfaceTypeConstraint(true).satisfy(test_interface->get_type()));
  EXPECT_TRUE(IsInterfaceTypeConstraint().satisfy(test_interface->get_type()));
  EXPECT_FALSE(IsInterfaceTypeConstraint(false).satisfy(
      test_class->get_interfaces()->at(0)));
  EXPECT_FALSE(IsInterfaceTypeConstraint().satisfy(test_class->get_type()));
  EXPECT_TRUE(
      IsInterfaceTypeConstraint(false).satisfy(type::java_lang_Object()));
  EXPECT_FALSE(IsInterfaceTypeConstraint(true).satisfy(type::java_lang_Void()));
}

TEST_F(TypeConstraintTest, TypeConstraintFromJson) {
  // TypeNameConstraint
  auto constraint = TypeConstraint::from_json(test::parse_json(
      R"({
        "constraint": "name",
        "pattern": "Landroid/util/Log;"
      })"));
  EXPECT_EQ(TypeNameConstraint("Landroid/util/Log;"), *constraint);

  EXPECT_THROW(
      TypeConstraint::from_json(test::parse_json(
          R"({
            "cOnstraint": "name",
            "pattern": "Landroid/util/Log;"
          })")),
      JsonValidationError);

  EXPECT_THROW(
      TypeConstraint::from_json(test::parse_json(
          R"({
            "constraint": "nAme",
            "pattern": "Landroid/util/Log;"
          })")),
      JsonValidationError);

  EXPECT_THROW(
      TypeConstraint::from_json(test::parse_json(
          R"({
            "constraint": "name",
            "paTtern": "Landroid/util/Log;"
          })")),
      JsonValidationError);
  // TypeNameConstraint

  // ExtendsConstraint
  {
    auto constraint = TypeConstraint::from_json(test::parse_json(
        R"({
          "constraint": "extends",
          "include_self": true,
          "inner": {
            "constraint": "name",
            "pattern": "Landroid/util/Log;"
          }
        })"));
    EXPECT_EQ(
        ExtendsConstraint(
            std::make_unique<TypeNameConstraint>("Landroid/util/Log;"),
            /* includes_self */ true),
        *constraint);
  }

  {
    auto constraint = TypeConstraint::from_json(test::parse_json(
        R"({
          "constraint": "extends",
          "inner": {
            "constraint": "name",
            "pattern": "Landroid/util/Log;"
          }
        })"));
    EXPECT_EQ(
        ExtendsConstraint(
            std::make_unique<TypeNameConstraint>("Landroid/util/Log;"),
            /* includes_self */ true),
        *constraint);
  }

  {
    auto constraint = TypeConstraint::from_json(test::parse_json(
        R"({
          "constraint": "extends",
          "include_self": false,
          "inner": {
            "constraint": "name",
            "pattern": "Landroid/util/Log;"
          }
        })"));
    EXPECT_EQ(
        ExtendsConstraint(
            std::make_unique<TypeNameConstraint>("Landroid/util/Log;"),
            /* includes_self */ false),
        *constraint);
  }

  EXPECT_THROW(
      TypeConstraint::from_json(test::parse_json(
          R"({
            "cOnstraint": "extends",
            "include_self": true,
            "inner": {
              "constraint": "name",
              "pattern": "Landroid/util/Log;"
            }
          })")),
      JsonValidationError);

  EXPECT_THROW(
      TypeConstraint::from_json(test::parse_json(
          R"({
            "constraint": "Extends",
            "include_self": true,
            "inner": {
              "constraint": "name",
              "pattern": "Landroid/util/Log;"
            }
          })")),
      JsonValidationError);

  EXPECT_THROW(
      TypeConstraint::from_json(test::parse_json(
          R"({
            "constraint": "extends",
            "include_self": true,
            "iNner": {
              "constraint": "name",
              "pattern": "Landroid/util/Log;"
            }
          })")),
      JsonValidationError);
  // ExtendsConstraint

  // SuperConstraint
  {
    auto constraint = TypeConstraint::from_json(test::parse_json(
        R"({
          "constraint": "super",
          "inner": {
            "constraint": "name",
            "pattern": "Landroid/util/Log;"
          }
        })"));
    EXPECT_EQ(
        SuperConstraint(
            std::make_unique<TypeNameConstraint>("Landroid/util/Log;")),
        *constraint);
  }

  EXPECT_THROW(
      TypeConstraint::from_json(test::parse_json(
          R"({
            "cOnstraint": "super",
            "inner": {
              "constraint": "name",
              "pattern": "Landroid/util/Log;"
            }
          })")),
      JsonValidationError);

  EXPECT_THROW(
      TypeConstraint::from_json(test::parse_json(
          R"({
            "constraint": "Super",
            "inner": {
              "constraint": "name",
              "pattern": "Landroid/util/Log;"
            }
          })")),
      JsonValidationError);

  EXPECT_THROW(
      TypeConstraint::from_json(test::parse_json(
          R"({
            "constraint": "super",
            "iNner": {
              "constraint": "name",
              "pattern": "Landroid/util/Log;"
            }
          })")),
      JsonValidationError);
  // SuperConstraint

  // HasAnnotationTypeConstraint
  {
    auto constraint = TypeConstraint::from_json(test::parse_json(
        R"({
          "constraint": "has_annotation",
          "type": "Lcom/facebook/Annotation;",
          "pattern": "A"
        })"));
    EXPECT_EQ(
        HasAnnotationTypeConstraint("Lcom/facebook/Annotation;", "A"),
        *constraint);
  }

  {
    auto constraint = TypeConstraint::from_json(test::parse_json(
        R"({
          "constraint": "has_annotation",
          "type": "Lcom/facebook/Annotation;"
        })"));
    EXPECT_EQ(
        HasAnnotationTypeConstraint("Lcom/facebook/Annotation;", std::nullopt),
        *constraint);
  }

  EXPECT_THROW(
      TypeConstraint::from_json(test::parse_json(
          R"({
            "Constraint": "has_annotation",
            "type": "Lcom/facebook/Annotation;",
            "pattern": "A"
          })")),
      JsonValidationError);

  EXPECT_THROW(
      TypeConstraint::from_json(test::parse_json(
          R"({
            "constraint": "Has_annotation",
            "type": "Lcom/facebook/Annotation;",
            "pattern": "A"
          })")),
      JsonValidationError);

  EXPECT_THROW(
      TypeConstraint::from_json(test::parse_json(
          R"({
            "constraint": "has_annotation",
            "Type": "Lcom/facebook/Annotation;",
            "pattern": "A"
          })")),
      JsonValidationError);

  EXPECT_THROW(
      TypeConstraint::from_json(test::parse_json(
          R"({
            "constraint": "Has_annotation",
            "type": "Lcom/facebook/Annotation;",
            "Pattern": "A"
          })")),
      JsonValidationError);
  // HasAnnotationTypeConstraint

  // IsClassTypeConstraint
  {
    auto constraint = TypeConstraint::from_json(test::parse_json(
        R"({
          "constraint": "is_class",
          "value": true
        })"));
    EXPECT_EQ(IsClassTypeConstraint(true), *constraint);
  }

  {
    auto constraint = TypeConstraint::from_json(test::parse_json(
        R"({
          "constraint": "is_class",
          "value": false
        })"));
    EXPECT_EQ(IsClassTypeConstraint(false), *constraint);
  }

  {
    auto constraint = TypeConstraint::from_json(test::parse_json(
        R"({
          "constraint": "is_class"
        })"));
    EXPECT_EQ(IsClassTypeConstraint(true), *constraint);
  }

  EXPECT_THROW(
      TypeConstraint::from_json(test::parse_json(
          R"({
          "constraint": "is_class",
          "value": "true"
        })")),
      JsonValidationError);
  // IsClassTypeConstraint

  // IsInterfaceTypeConstraint
  {
    auto constraint = TypeConstraint::from_json(test::parse_json(
        R"({
          "constraint": "is_interface",
          "value": true
        })"));
    EXPECT_EQ(IsInterfaceTypeConstraint(true), *constraint);
  }

  {
    auto constraint = TypeConstraint::from_json(test::parse_json(
        R"({
          "constraint": "is_interface",
          "value": false
        })"));
    EXPECT_EQ(IsInterfaceTypeConstraint(false), *constraint);
  }

  {
    auto constraint = TypeConstraint::from_json(test::parse_json(
        R"({
          "constraint": "is_interface"
        })"));
    EXPECT_EQ(IsInterfaceTypeConstraint(true), *constraint);
  }

  EXPECT_THROW(
      TypeConstraint::from_json(test::parse_json(
          R"({
          "constraint": "is_interface",
          "value": "true"
        })")),
      JsonValidationError);
  // IsInterfaceTypeConstraint
}

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
class MethodConstraintTest : public test::Test {};
} // namespace

TEST_F(MethodConstraintTest, TypeNameConstraintSatisfy) {
  std::string class_name = "Landroid/util/Log;";
  auto type = DexType::make_type(DexString::make_string(class_name));

  EXPECT_TRUE(TypeNameConstraint(class_name).satisfy(type));
  EXPECT_TRUE(TypeNameConstraint("([A-Za-z/]*/?)+;").satisfy(type));
  EXPECT_FALSE(TypeNameConstraint("Landroid/util/Log").satisfy(type));
  EXPECT_FALSE(TypeNameConstraint("([A-Za-z/]*/)+;").satisfy(type));
}

TEST_F(MethodConstraintTest, MethodNameConstraintSatisfy) {
  Scope scope;
  std::string method_name = "println";
  auto context = test::make_empty_context();

  auto* method = context.methods->create(
      redex::create_void_method(scope, "method", method_name));

  EXPECT_TRUE(MethodNameConstraint(method_name).satisfy(method));
  EXPECT_TRUE(MethodNameConstraint("[A-Za-z]+").satisfy(method));
  EXPECT_FALSE(MethodNameConstraint("printLn").satisfy(method));
  EXPECT_FALSE(MethodNameConstraint("[0-9]+").satisfy(method));
}

TEST_F(MethodConstraintTest, ParentConstraintSatisfy) {
  Scope scope;
  std::string class_name = "Landroid/util/Log;";
  auto context = test::make_empty_context();
  auto* method = context.methods->create(
      redex::create_void_method(scope, class_name, "method"));

  EXPECT_TRUE(ParentConstraint(std::make_unique<TypeNameConstraint>(class_name))
                  .satisfy(method));

  EXPECT_FALSE(ParentConstraint(
                   std::make_unique<TypeNameConstraint>("Landroid/util/Log"))
                   .satisfy(method));
}

TEST_F(MethodConstraintTest, AllOfMethodConstraintSatisfy) {
  Scope scope;
  auto context = test::make_empty_context();

  std::string class_name = "Landroid/util/Log;";
  std::string method_name = "println";
  auto* method = context.methods->create(
      redex::create_void_method(scope, class_name, method_name));

  EXPECT_TRUE(AllOfMethodConstraint({}).satisfy(method));

  {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(std::make_unique<MethodNameConstraint>(method_name));

    EXPECT_TRUE(AllOfMethodConstraint(std::move(constraints)).satisfy(method));
  }

  {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(std::make_unique<MethodNameConstraint>(method_name));
    constraints.push_back(std::make_unique<ParentConstraint>(
        std::make_unique<TypeNameConstraint>(class_name)));

    EXPECT_TRUE(AllOfMethodConstraint(std::move(constraints)).satisfy(method));
  }

  {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(std::make_unique<MethodNameConstraint>("printLn"));

    EXPECT_FALSE(AllOfMethodConstraint(std::move(constraints)).satisfy(method));
  }

  {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(std::make_unique<MethodNameConstraint>(method_name));
    constraints.push_back(std::make_unique<ParentConstraint>(
        std::make_unique<TypeNameConstraint>("Landroid/util/Log")));

    EXPECT_FALSE(AllOfMethodConstraint(std::move(constraints)).satisfy(method));
  }
}

TEST_F(MethodConstraintTest, AnyOfMethodConstraintSatisfy) {
  Scope scope;
  auto context = test::make_empty_context();

  std::string class_name = "Landroid/util/Log;";
  std::string method_name = "println";
  auto* method = context.methods->create(
      redex::create_void_method(scope, class_name, method_name));

  EXPECT_TRUE(AnyOfMethodConstraint({}).satisfy(method));

  {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(std::make_unique<MethodNameConstraint>(method_name));

    EXPECT_TRUE(AnyOfMethodConstraint(std::move(constraints)).satisfy(method));
  }

  {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(std::make_unique<MethodNameConstraint>(method_name));
    constraints.push_back(std::make_unique<ParentConstraint>(
        std::make_unique<TypeNameConstraint>(class_name)));

    EXPECT_TRUE(AnyOfMethodConstraint(std::move(constraints)).satisfy(method));
  }

  {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(std::make_unique<MethodNameConstraint>(method_name));

    EXPECT_TRUE(AnyOfMethodConstraint(std::move(constraints)).satisfy(method));
  }

  {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(std::make_unique<MethodNameConstraint>("printLn"));
    constraints.push_back(std::make_unique<ParentConstraint>(
        std::make_unique<TypeNameConstraint>("Landroid/util/Log")));

    EXPECT_FALSE(AnyOfMethodConstraint(std::move(constraints)).satisfy(method));
  }
}

TEST_F(MethodConstraintTest, NotMethodConstraintSatisfy) {
  Scope scope;
  auto context = test::make_empty_context();

  std::string class_name = "Landroid/util/Log;";
  std::string method_name = "println";
  auto* method = context.methods->create(
      redex::create_void_method(scope, class_name, method_name));

  EXPECT_TRUE(
      NotMethodConstraint(std::make_unique<MethodNameConstraint>("printLn"))
          .satisfy(method));

  EXPECT_FALSE(
      NotMethodConstraint(std::make_unique<MethodNameConstraint>(method_name))
          .satisfy(method));
}

TEST_F(MethodConstraintTest, NumberParametersConstraintSatisfy) {
  Scope scope;
  auto context = test::make_empty_context();
  auto methods = redex::create_methods(
      scope,
      "LClass;",
      {
          R"(
            (method (public static) "LClass;.method_1:(III)V"
            (
              (return-void)
            )
            ))",
          R"(
            (method (public) "LClass;.method_2:(II)V"
            (
              (return-void)
            )
            ))",
          R"(
            (method (public) "LClass;.method_3:(I)V"
            (
              (return-void)
            )
            ))",
      });
  auto constraint = NumberParametersConstraint(
      IntegerConstraint(3, IntegerConstraint::Operator::EQ));

  EXPECT_TRUE(constraint.satisfy(context.methods->create(methods[0])));
  EXPECT_TRUE(constraint.satisfy(context.methods->create(methods[1])));
  EXPECT_FALSE(constraint.satisfy(context.methods->create(methods[2])));
}

TEST_F(MethodConstraintTest, NumberOverridesConstraintSatisfy) {
  Scope scope;
  auto* dex_base_method = redex::create_void_method(
      scope,
      /* class_name */ "LClass;",
      /* method */ "onReceive",
      /* parameter_type */ "Landroid/content/Context;Landroid/content/Intent;",
      /* return_type */ "V");
  auto* dex_first_overriding_method = redex::create_void_method(
      scope,
      /* class_name */ "LSubclass;",
      /* method */ "onReceive",
      /* parameter_type */ "Landroid/content/Context;Landroid/content/Intent;",
      /* return_type */ "V",
      /* super */ dex_base_method->get_class());
  auto* dex_second_overriding_method = redex::create_void_method(
      scope,
      /* class_name */ "LSubSubclass;",
      /* method */ "onReceive",
      /* parameter_type */ "Landroid/content/Context;Landroid/content/Intent;",
      /* return_type */ "V",
      /* super */ dex_first_overriding_method->get_class());
  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  auto* base_method = context.methods->get(dex_base_method);
  auto* first_overriding_method =
      context.methods->get(dex_first_overriding_method);
  auto* second_overriding_method =
      context.methods->get(dex_second_overriding_method);

  auto constraint_one = NumberOverridesConstraint(
      IntegerConstraint(1, IntegerConstraint::Operator::EQ), context);
  auto constraint_two = NumberOverridesConstraint(
      IntegerConstraint(2, IntegerConstraint::Operator::EQ), context);

  EXPECT_TRUE(constraint_two.satisfy(base_method));
  EXPECT_TRUE(constraint_one.satisfy(first_overriding_method));
  EXPECT_FALSE(constraint_one.satisfy(second_overriding_method));
  EXPECT_FALSE(constraint_one.satisfy(base_method));
}

TEST_F(MethodConstraintTest, IsStaticConstraintSatisfy) {
  Scope scope;
  auto context = test::make_empty_context();
  auto methods = redex::create_methods(
      scope,
      "LClass;",
      {
          R"(
            (method (public static) "LClass;.method_1:()V"
            (
              (return-void)
            )
            ))",
          R"(
            (method (public) "LClass;.method_2:()V"
            (
              (return-void)
            )
            ))",
      });
  auto constraint = IsStaticConstraint(false);

  EXPECT_FALSE(constraint.satisfy(context.methods->create(methods[0])));
  EXPECT_TRUE(constraint.satisfy(context.methods->create(methods[1])));
}

TEST_F(MethodConstraintTest, IsNativeConstraintSatisfy) {
  Scope scope;
  auto context = test::make_empty_context();
  auto methods = redex::create_methods(
      scope,
      "LClass;",
      {
          R"(
            (method (public) "LClass;.non_native:()V"
            (
              (return-void)
            )
            ))",
          R"(
            (method (public native) "LClass;.native:()V"
            (
              (return-void)
            )
            ))",
      });

  EXPECT_FALSE(
      IsNativeConstraint(true).satisfy(context.methods->create(methods[0])));
  EXPECT_TRUE(
      IsNativeConstraint(true).satisfy(context.methods->create(methods[1])));

  EXPECT_TRUE(
      IsNativeConstraint(false).satisfy(context.methods->create(methods[0])));
  EXPECT_FALSE(
      IsNativeConstraint(false).satisfy(context.methods->create(methods[1])));
}

TEST_F(MethodConstraintTest, HasCodeConstraintSatisfy) {
  Scope scope;
  auto context = test::make_empty_context();
  auto methods = redex::create_methods(scope, "LClass;", {R"(
            (method (public) "LClass;.method_1:()V"
              (
                (return-void)
              )
            ))"});

  EXPECT_TRUE(
      HasCodeConstraint(true).satisfy(context.methods->create(methods[0])));
  EXPECT_FALSE(
      HasCodeConstraint(false).satisfy(context.methods->create(methods[0])));
}

TEST_F(MethodConstraintTest, ParameterConstraintSatisfy) {
  Scope scope;
  auto context = test::make_empty_context();
  auto methods = redex::create_methods(
      scope,
      "LClass;",
      {
          R"(
            (method (public static) "LClass;.method_1:(Landroid/content/Intent;I)V"
            (
              (return-void)
            )
            ))",
          R"(
            (method (public) "LClass;.method_2:(ILandroid/content/Intent;)V"
            (
              (return-void)
            )
            ))",
      });
  auto constraint = ParameterConstraint(
      0, std::make_unique<TypeNameConstraint>("Landroid/content/Intent;"));

  EXPECT_TRUE(constraint.satisfy(context.methods->create(methods[0])));
  EXPECT_FALSE(constraint.satisfy(context.methods->create(methods[1])));
}

TEST_F(MethodConstraintTest, SignatureConstraintSatisfy) {
  Scope scope;
  auto context = test::make_empty_context();
  auto methods = redex::create_methods(
      scope,
      "Landroid/app/Fragment;",
      {
          R"(
            (method (public) "Landroid/app/Fragment;.getArguments:()V"
            (
              (return-void)
            )
            ))",
          R"(
            (method (public) "Landroid/app/Fragment;.getArguments:(I)V"
            (
              (return-void)
            )
            ))",
      });
  auto constraint =
      SignatureConstraint("Landroid/app/Fragment;\\.getArguments:\\(\\)V");

  EXPECT_TRUE(constraint.satisfy(context.methods->create(methods[0])));
  EXPECT_FALSE(constraint.satisfy(context.methods->create(methods[1])));
}

TEST_F(MethodConstraintTest, ExtendsConstraintSatisfy) {
  std::string class_name = "Landroid/util/Log;";
  ClassCreator creator(DexType::make_type(DexString::make_string(class_name)));
  creator.set_super(type::java_lang_Object());
  auto test_class = creator.create();

  EXPECT_TRUE(ExtendsConstraint(
                  std::make_unique<TypeNameConstraint>(class_name),
                  /* includes_self */ true)
                  .satisfy(test_class->get_type()));

  EXPECT_TRUE(ExtendsConstraint(
                  std::make_unique<TypeNameConstraint>("Ljava/lang/Object;"),
                  /* includes_self */ true)
                  .satisfy(test_class->get_type()));

  EXPECT_TRUE(ExtendsConstraint(
                  std::make_unique<TypeNameConstraint>("Ljava/lang/Object;"),
                  /* includes_self */ false)
                  .satisfy(test_class->get_type()));

  EXPECT_FALSE(ExtendsConstraint(
                   std::make_unique<TypeNameConstraint>(class_name),
                   /* includes_self */ true)
                   .satisfy(type::java_lang_Object()));

  EXPECT_FALSE(ExtendsConstraint(
                   std::make_unique<TypeNameConstraint>("Landroid/util/Log"),
                   /* includes_self */ true)
                   .satisfy(test_class->get_type()));

  EXPECT_FALSE(ExtendsConstraint(
                   std::make_unique<TypeNameConstraint>(class_name),
                   /* includes_self */ false)
                   .satisfy(test_class->get_type()));

  EXPECT_FALSE(ExtendsConstraint(
                   std::make_unique<TypeNameConstraint>("Ljava/lang/Object;"),
                   /* includes_self */ false)
                   .satisfy(type::java_lang_Object()));
}

TEST_F(MethodConstraintTest, SuperConstraintSatisfy) {
  std::string class_name = "Landroid/util/Log;";
  std::string super_class_name = "Landroid/util/LogBase;";

  ClassCreator creator(DexType::make_type(DexString::make_string(class_name)));
  ClassCreator super_creator(
      DexType::make_type(DexString::make_string(super_class_name)));

  super_creator.set_super(type::java_lang_Object());
  auto super_class = super_creator.create();

  creator.set_super(super_class->get_type());
  auto test_class = creator.create();

  EXPECT_FALSE(SuperConstraint(std::make_unique<TypeNameConstraint>(class_name))
                   .satisfy(test_class->get_type()));

  EXPECT_TRUE(
      SuperConstraint(std::make_unique<TypeNameConstraint>(super_class_name))
          .satisfy(test_class->get_type()));

  EXPECT_FALSE(SuperConstraint(
                   std::make_unique<TypeNameConstraint>("Ljava/lang/Object;"))
                   .satisfy(test_class->get_type()));

  EXPECT_FALSE(SuperConstraint(std::make_unique<TypeNameConstraint>(class_name))
                   .satisfy(super_class->get_type()));

  EXPECT_FALSE(SuperConstraint(std::make_unique<TypeNameConstraint>(
                                   "Landroid/util/LogBase"))
                   .satisfy(test_class->get_type()));
}

TEST_F(MethodConstraintTest, ReturnConstraint) {
  Scope scope;
  auto context = test::make_empty_context();
  auto methods = redex::create_methods(
      scope,
      "LClass;",
      {
          R"(
            (method (public static) "LClass;.method_1:(I)I"
            (
              (return-void)
            )
            ))",
          R"(
            (method (public) "LClass;.method_2:(I)Landroid/util/Log;"
            (
              (return-void)
            )
            ))",
      });

  EXPECT_TRUE(ReturnConstraint(std::make_unique<TypeNameConstraint>("I"))
                  .satisfy(context.methods->create(methods[0])));

  EXPECT_TRUE(ReturnConstraint(
                  std::make_unique<TypeNameConstraint>("Landroid/util/Log;"))
                  .satisfy(context.methods->create(methods[1])));

  EXPECT_FALSE(ReturnConstraint(std::make_unique<TypeNameConstraint>("V"))
                   .satisfy(context.methods->create(methods[1])));
}

TEST_F(MethodConstraintTest, VisibilityMethodConstraint) {
  Scope scope;
  auto context = test::make_empty_context();
  auto methods = redex::create_methods(
      scope,
      "LClass;",
      {
          R"(
            (method (public) "LClass;.public:(I)I"
            (
              (return-void)
            )
            ))",
          R"(
            (method (private) "LClass;.private:(I)I"
            (
              (return-void)
            )
            ))",
          R"(
            (method (protected) "LClass;.protected:(I)I"
            (
              (return-void)
            )
            ))",
      });

  EXPECT_TRUE(VisibilityMethodConstraint(ACC_PUBLIC)
                  .satisfy(context.methods->create(methods[0])));
  EXPECT_FALSE(VisibilityMethodConstraint(ACC_PUBLIC)
                   .satisfy(context.methods->create(methods[1])));
  EXPECT_FALSE(VisibilityMethodConstraint(ACC_PUBLIC)
                   .satisfy(context.methods->create(methods[2])));

  EXPECT_FALSE(VisibilityMethodConstraint(ACC_PRIVATE)
                   .satisfy(context.methods->create(methods[0])));
  EXPECT_TRUE(VisibilityMethodConstraint(ACC_PRIVATE)
                  .satisfy(context.methods->create(methods[1])));
  EXPECT_FALSE(VisibilityMethodConstraint(ACC_PRIVATE)
                   .satisfy(context.methods->create(methods[2])));

  EXPECT_FALSE(VisibilityMethodConstraint(ACC_PROTECTED)
                   .satisfy(context.methods->create(methods[0])));
  EXPECT_FALSE(VisibilityMethodConstraint(ACC_PROTECTED)
                   .satisfy(context.methods->create(methods[1])));
  EXPECT_TRUE(VisibilityMethodConstraint(ACC_PROTECTED)
                  .satisfy(context.methods->create(methods[2])));
}

TEST_F(MethodConstraintTest, MethodConstraintFromJson) {
  auto context = test::make_empty_context();

  // MethodNameConstraint
  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "name",
          "pattern": "println"
        })"),
        context);
    EXPECT_EQ(MethodNameConstraint("println"), *constraint);
  }

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "cOnstraint": "name",
            "pattern": "println"
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "nAme",
            "pattern": "println"
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "name",
            "paTtern": "println"
          })"),
          context),
      JsonValidationError);
  // MethodNameConstraint

  // ParentConstraint
  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "parent",
          "inner": {
            "constraint": "name",
            "pattern": "Landroid/util/Log;"
          }
        })"),
        context);
    EXPECT_EQ(
        ParentConstraint(
            std::make_unique<TypeNameConstraint>("Landroid/util/Log;")),
        *constraint);
  }

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "cOnstraint": "parent",
            "inner": {
              "constraint": "name",
              "pattern": "Landroid/util/Log;"
            }
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "pArent",
            "inner": {
              "constraint": "name",
              "pattern": "Landroid/util/Log;"
            }
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "parent",
            "iNner": {
              "constraint": "name",
              "pattern": "Landroid/util/Log;"
            }
          })"),
          context),
      JsonValidationError);
  // ParentConstraint

  // NumberParametersConstraint
  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "number_parameters",
          "inner": {
            "constraint": "==",
            "value": 3
          }
        })"),
        context);
    EXPECT_EQ(
        NumberParametersConstraint(
            IntegerConstraint(3, IntegerConstraint::Operator::EQ)),
        *constraint);
  }

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "cOnstraint": "number_parameters",
            "inner": {
              "constraint": "==",
              "value": 3
            }
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "nUmber_parameters",
            "inner": {
              "constraint": "==",
              "value": 3
            }
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "number_parameters",
            "iNner": {
              "constraint": "==",
              "value": 3
            }
          })"),
          context),
      JsonValidationError);
  // NumberParametersConstraint

  // NumberOverridesConstraint
  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "number_overrides",
          "inner": {
            "constraint": "==",
            "value": 3
          }
        })"),
        context);
    EXPECT_EQ(
        NumberOverridesConstraint(
            IntegerConstraint(3, IntegerConstraint::Operator::EQ), context),
        *constraint);
  }

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "cOnstraint": "number_overrides",
            "inner": {
              "constraint": "==",
              "value": 3
            }
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "nUmber_overrides",
            "inner": {
              "constraint": "==",
              "value": 3
            }
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "number_overrides",
            "iNner": {
              "constraint": "==",
              "value": 3
            }
          })"),
          context),
      JsonValidationError);
  // NumberParametersConstraint

  // IsStaticConstraint
  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "is_static",
          "value": false
        })"),
        context);
    EXPECT_EQ(IsStaticConstraint(false), *constraint);
  }

  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "is_static",
          "vAlue": false
        })"),
        context);
    EXPECT_EQ(IsStaticConstraint(true), *constraint);
  }

  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "is_static"
        })"),
        context);
    EXPECT_EQ(IsStaticConstraint(true), *constraint);
  }

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "cOnstraint": "is_static",
            "value": false
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "is_Static",
            "value": false
          })"),
          context),
      JsonValidationError);
  // IsStaticConstraint

  // IsConstructorConstraint
  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "is_constructor",
          "value": false
        })"),
        context);
    EXPECT_EQ(IsConstructorConstraint(false), *constraint);
  }

  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "is_constructor",
          "vAlue": false
        })"),
        context);
    EXPECT_EQ(IsConstructorConstraint(true), *constraint);
  }

  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "is_constructor"
        })"),
        context);
    EXPECT_EQ(IsConstructorConstraint(true), *constraint);
  }

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "cOnstraint": "is_constructor",
            "value": false
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "is_Constructor",
            "value": false
          })"),
          context),
      JsonValidationError);
  // IsConstructorConstraint

  // IsNativeConstraint
  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "is_native",
          "value": false
        })"),
        context);
    EXPECT_EQ(IsNativeConstraint(false), *constraint);
  }

  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "is_native",
          "vAlue": false
        })"),
        context);
    EXPECT_EQ(IsNativeConstraint(true), *constraint);
  }

  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "is_native"
        })"),
        context);
    EXPECT_EQ(IsNativeConstraint(true), *constraint);
  }

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "cOnstraint": "is_native",
            "value": false
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "is_Native",
            "value": false
          })"),
          context),
      JsonValidationError);
  // IsNativeConstraint

  // HasCodeConstraint
  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "has_code",
          "value": false
        })"),
        context);
    EXPECT_EQ(HasCodeConstraint(false), *constraint);
  }

  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "has_code",
          "vAlue": false
        })"),
        context);
    EXPECT_EQ(HasCodeConstraint(true), *constraint);
  }

  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "has_code"
        })"),
        context);
    EXPECT_EQ(HasCodeConstraint(true), *constraint);
  }

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "cOnstraint": "has_code",
            "value": false
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "Has_code",
            "value": false
          })"),
          context),
      JsonValidationError);
  // HasCodeConstraint

  // HasAnnotationMethodConstraint
  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "has_annotation",
          "type": "Lcom/facebook/Annotation;",
          "pattern": "A"
        })"),
        context);
    EXPECT_EQ(
        HasAnnotationMethodConstraint("Lcom/facebook/Annotation;", "A"),
        *constraint);
  }

  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "has_annotation",
          "type": "Lcom/facebook/Annotation;"
        })"),
        context);
    EXPECT_EQ(
        HasAnnotationMethodConstraint(
            "Lcom/facebook/Annotation;", std::nullopt),
        *constraint);
  }

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "Constraint": "has_annotation",
            "type": "Lcom/facebook/Annotation;",
            "pattern": "A"
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "Has_annotation",
            "type": "Lcom/facebook/Annotation;",
            "pattern": "A"
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "has_annotation",
            "Type": "Lcom/facebook/Annotation;",
            "pattern": "A"
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "Has_annotation",
            "type": "Lcom/facebook/Annotation;",
            "Pattern": "A"
          })"),
          context),
      JsonValidationError);
  // HasAnnotationMethodConstraint

  // ParameterConstraint
  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "parameter",
          "idx": 0,
          "inner": {
            "constraint": "name",
            "pattern": "Landroid/content/Intent;"
          }
        })"),
        context);
    EXPECT_EQ(
        ParameterConstraint(
            0,
            std::make_unique<TypeNameConstraint>("Landroid/content/Intent;")),
        *constraint);
  }

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "cOnstraint": "parameter",
            "idx": 0,
            "inner": {
              "constraint": "name",
              "pattern": "Landroid/content/Intent;"
            }
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "paRameter",
            "idx": 0,
            "inner": {
              "constraint": "name",
              "pattern": "Landroid/content/Intent;"
            }
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "parameter",
            "iDx": 0,
            "inner": {
              "constraint": "name",
              "pattern": "Landroid/content/Intent;"
            }
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "parameter",
            "idx": 0,
            "innEr": {
              "constraint": "name",
              "pattern": "Landroid/content/Intent;"
            }
          })"),
          context),
      JsonValidationError);
  // ParameterConstraint

  // SignatureConstraint
  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "signature",
          "pattern": "Landroid/app/Activity;\\.getIntent:\\(\\)Landroid/content/Intent;"
        })"),
        context);
    EXPECT_EQ(
        SignatureConstraint(
            "Landroid/app/Activity;\\.getIntent:\\(\\)Landroid/content/Intent;"),
        *constraint);
  }

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "cOnstraint": "signature",
            "pattern": "Landroid/app/Activity;\\.getIntent:\\(\\)Landroid/content/Intent;"
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "sIgnature",
            "pattern": "Landroid/app/Activity;\\.getIntent:\\(\\)Landroid/content/Intent;"
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "signature",
            "pAttern": "Landroid/app/Activity;\\.getIntent:\\(\\)Landroid/content/Intent;"
          })"),
          context),
      JsonValidationError);
  // SignatureConstraint

  // AnyOfMethodConstraint
  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "any_of",
          "inners": [
            {
              "constraint": "signature",
              "pattern": "Landroidx/fragment/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"
            },
            {
              "constraint": "signature",
              "pattern": "Landroid/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"
            }
          ]
        })"),
        context);

    {
      std::vector<std::unique_ptr<MethodConstraint>> constraints;
      constraints.push_back(std::make_unique<SignatureConstraint>(
          "Landroidx/fragment/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"));
      constraints.push_back(std::make_unique<SignatureConstraint>(
          "Landroid/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"));

      EXPECT_EQ(AnyOfMethodConstraint(std::move(constraints)), *constraint);
    }

    {
      std::vector<std::unique_ptr<MethodConstraint>> constraints;
      constraints.push_back(std::make_unique<SignatureConstraint>(
          "Landroid/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"));
      constraints.push_back(std::make_unique<SignatureConstraint>(
          "Landroidx/fragment/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"));

      EXPECT_EQ(AnyOfMethodConstraint(std::move(constraints)), *constraint);
    }
  }

  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "any_of",
          "inNers": [
            {
              "constraint": "signature",
              "pattern": "Landroidx/fragment/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"
            },
            {
              "constraint": "signature",
              "pattern": "Landroid/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"
            }
          ]
        })"),
        context);
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    EXPECT_EQ(AnyOfMethodConstraint(std::move(constraints)), *constraint);
  }

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "Constraint": "any_of",
            "inners": [
              {
                "Constraint": "signature",
                "pattern": "Landroidx/fragment/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"
              },
              {
                "constraint": "signature",
                "pattern": "Landroid/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"
              }
            ]
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "Any_of",
            "inners": [
              {
                "constraint": "signature",
                "pattern": "Landroidx/fragment/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"
              },
              {
                "constraint": "signature",
                "pattern": "Landroid/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"
              }
            ]
          })"),
          context),
      JsonValidationError);
  // AnyOfMethodConstraint

  // AllOfMethodConstraint
  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "all_of",
          "inners": [
            {
              "constraint": "name",
              "pattern": "println"
            },
            {
              "constraint": "parent",
              "inner": {
                "constraint": "name",
                "pattern": "Landroid/util/Log;"
              }
            }
          ]
        })"),
        context);

    {
      std::vector<std::unique_ptr<MethodConstraint>> constraints;
      constraints.push_back(std::make_unique<MethodNameConstraint>("println"));
      constraints.push_back(std::make_unique<ParentConstraint>(
          std::make_unique<TypeNameConstraint>("Landroid/util/Log;")));

      EXPECT_EQ(AllOfMethodConstraint(std::move(constraints)), *constraint);
    }

    {
      std::vector<std::unique_ptr<MethodConstraint>> constraints;
      constraints.push_back(std::make_unique<ParentConstraint>(
          std::make_unique<TypeNameConstraint>("Landroid/util/Log;")));
      constraints.push_back(std::make_unique<MethodNameConstraint>("println"));

      EXPECT_EQ(AllOfMethodConstraint(std::move(constraints)), *constraint);
    }
  }

  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "all_of",
          "inNers": [
            {
              "constraint": "signature",
              "pattern": "Landroidx/fragment/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"
            },
            {
              "constraint": "signature",
              "pattern": "Landroid/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"
            }
          ]
        })"),
        context);
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    EXPECT_EQ(AllOfMethodConstraint(std::move(constraints)), *constraint);
  }

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "Constraint": "any_of",
            "inners": [
              {
                "Constraint": "signature",
                "pattern": "Landroidx/fragment/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"
              },
              {
                "constraint": "signature",
                "pattern": "Landroid/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"
              }
            ]
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "All_of",
            "inners": [
              {
                "constraint": "signature",
                "pattern": "Landroidx/fragment/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"
              },
              {
                "constraint": "signature",
                "pattern": "Landroid/app/Fragment;\\.getArguments:\\(\\)Landroid/os/Bundle;"
              }
            ]
          })"),
          context),
      JsonValidationError);
  // AllOfMethodConstraint

  // ReturnConstraint
  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "return",
          "inner": {
            "constraint": "name",
            "pattern": "Landroid/util/Log;"
          }
        })"),
        context);
    EXPECT_EQ(
        ReturnConstraint(
            std::make_unique<TypeNameConstraint>("Landroid/util/Log;")),
        *constraint);
  }

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "cOnstraint": "return",
            "inner": {
              "constraint": "name",
              "pattern": "Landroid/util/Log;"
            }
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "Return",
            "inner": {
              "constraint": "name",
              "pattern": "Landroid/util/Log;"
            }
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "return",
            "iNner": {
              "constraint": "name",
              "pattern": "Landroid/util/Log;"
            }
          })"),
          context),
      JsonValidationError);
  // ReturnConstraint

  // VisibilityMethodConstraint
  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "visibility",
          "is": "public"
        })"),
        context);
    EXPECT_EQ(VisibilityMethodConstraint(ACC_PUBLIC), *constraint);
  }

  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "visibility",
          "is": "private"
        })"),
        context);
    EXPECT_EQ(VisibilityMethodConstraint(ACC_PRIVATE), *constraint);
  }

  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "visibility",
          "is": "protected"
        })"),
        context);
    EXPECT_EQ(VisibilityMethodConstraint(ACC_PROTECTED), *constraint);
  }

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "cOnstraint": "visibility",
            "is": "public"
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "Visibility",
            "is": "public"
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "visibility",
            "Is": "public"
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "visibility",
            "is": "unknown"
          })"),
          context),
      JsonValidationError);
  // VisibilityMethodConstraint

  // NotMethodConstraint
  {
    auto constraint = MethodConstraint::from_json(
        test::parse_json(
            R"({
          "constraint": "not",
          "inner": {
            "constraint": "name",
            "pattern": "Landroid/widget/EditText;"
          }
        })"),
        context);
    EXPECT_EQ(
        NotMethodConstraint(std::make_unique<MethodNameConstraint>(
            "Landroid/widget/EditText;")),
        *constraint);
  }

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "cOnstraint": "not",
            "inner": {
              "constraint": "name",
              "pattern": "Landroid/widget/EditText;"
            }
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "Not",
            "inner": {
              "constraint": "name",
              "pattern": "Landroid/widget/EditText;"
            }
          })"),
          context),
      JsonValidationError);

  EXPECT_THROW(
      MethodConstraint::from_json(
          test::parse_json(
              R"({
            "constraint": "not",
            "Inner": {
              "constraint": "name",
              "pattern": "Landroid/widget/EditText;"
            }
          })"),
          context),
      JsonValidationError);
  // NotMethodConstraint
}

TEST_F(MethodConstraintTest, MethodNameConstraintMaySatisfy) {
  Scope scope;
  auto* method_a =
      redex::create_void_method(scope, "class_name", "method_name_a");
  auto* method_b =
      redex::create_void_method(scope, "class_name_b", "method_name_b");
  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  MethodMappings method_mappings{*context.methods};

  EXPECT_EQ(
      MethodNameConstraint("method_name_a").may_satisfy(method_mappings),
      marianatrench::MethodHashedSet({context.methods->get(method_a)}));
  EXPECT_EQ(
      MethodNameConstraint("method_name_b").may_satisfy(method_mappings),
      marianatrench::MethodHashedSet({context.methods->get(method_b)}));
  EXPECT_TRUE(MethodNameConstraint("method_name_nonexistent")
                  .may_satisfy(method_mappings)
                  .is_bottom());
  EXPECT_TRUE(MethodNameConstraint("method_name_*")
                  .may_satisfy(method_mappings)
                  .is_top());
}

TEST_F(MethodConstraintTest, ParentConstraintMaySatisfy) {
  Scope scope;
  auto* method_a =
      redex::create_void_method(scope, "LClass;", "method_name_a", "", "V");
  auto* method_b = redex::create_void_method(
      scope, "LSubClass;", "method_name_b", "", "V", method_a->get_class());
  DexStore store("test-stores");
  auto interface = DexType::make_type("LInterface;");
  ClassCreator creator(interface);
  creator.set_access(DexAccessFlags::ACC_INTERFACE);
  creator.set_super(type::java_lang_Object());
  creator.create();
  auto super_interface = DexType::make_type("LSuperInterface;");
  ClassCreator super_creator(super_interface);
  super_creator.set_access(DexAccessFlags::ACC_INTERFACE);
  super_creator.set_super(type::java_lang_Object());
  super_creator.create();

  type_class(method_a->get_class())
      ->set_interfaces(DexTypeList::make_type_list({interface}));
  type_class(interface)->set_interfaces(
      DexTypeList::make_type_list({super_interface}));
  store.add_classes(scope);
  auto context = test::make_context(store);
  MethodMappings method_mappings{*context.methods};

  EXPECT_EQ(
      ParentConstraint(std::make_unique<TypeNameConstraint>("LClass;"))
          .may_satisfy(method_mappings),
      marianatrench::MethodHashedSet({context.methods->get(method_a)}));

  EXPECT_TRUE(ParentConstraint(std::make_unique<TypeNameConstraint>(
                                   "class_name_nonexistant"))
                  .may_satisfy(method_mappings)
                  .is_bottom());

  EXPECT_TRUE(
      ParentConstraint(std::make_unique<TypeNameConstraint>("L(Sub)?Class;"))
          .may_satisfy(method_mappings)
          .is_top());

  /* With Extends */
  EXPECT_EQ(
      ParentConstraint(std::make_unique<ExtendsConstraint>(
                           std::make_unique<TypeNameConstraint>("LClass;")))
          .may_satisfy(method_mappings),
      marianatrench::MethodHashedSet(
          {context.methods->get(method_a), context.methods->get(method_b)}));

  EXPECT_EQ(
      ParentConstraint(std::make_unique<ExtendsConstraint>(
                           std::make_unique<TypeNameConstraint>("LSubClass;")))
          .may_satisfy(method_mappings),
      marianatrench::MethodHashedSet({context.methods->get(method_b)}));

  EXPECT_EQ(
      ParentConstraint(std::make_unique<ExtendsConstraint>(
                           std::make_unique<TypeNameConstraint>("LInterface;")))
          .may_satisfy(method_mappings),
      marianatrench::MethodHashedSet(
          {context.methods->get(method_a), context.methods->get(method_b)}));

  EXPECT_EQ(
      ParentConstraint(
          std::make_unique<ExtendsConstraint>(
              std::make_unique<TypeNameConstraint>("LSuperInterface;")))
          .may_satisfy(method_mappings),
      marianatrench::MethodHashedSet(
          {context.methods->get(method_a), context.methods->get(method_b)}));

  EXPECT_TRUE(ParentConstraint(std::make_unique<ExtendsConstraint>(
                                   std::make_unique<TypeNameConstraint>(
                                       "class_name_nonexistant")))
                  .may_satisfy(method_mappings)
                  .is_bottom());
}

TEST_F(MethodConstraintTest, AllOfMethodConstraintMaySatisfy) {
  Scope scope;
  auto* method_a =
      redex::create_void_method(scope, "class_name", "method_name_a");
  redex::create_void_method(scope, "class_name_b", "method_name_b");
  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  MethodMappings method_mappings{*context.methods};

  EXPECT_TRUE(AllOfMethodConstraint({}).may_satisfy(method_mappings).is_top());

  {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(
        std::make_unique<MethodNameConstraint>("method_name_a"));

    EXPECT_EQ(
        AllOfMethodConstraint(std::move(constraints))
            .may_satisfy(method_mappings),
        marianatrench::MethodHashedSet({context.methods->get(method_a)}));
  }

  {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(
        std::make_unique<MethodNameConstraint>("method_name_a"));
    constraints.push_back(
        std::make_unique<MethodNameConstraint>("method_name_b"));

    EXPECT_EQ(
        AllOfMethodConstraint(std::move(constraints))
            .may_satisfy(method_mappings),
        marianatrench::MethodHashedSet());
  }

  {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(
        std::make_unique<MethodNameConstraint>("method_name_nonexistant"));

    EXPECT_TRUE(AllOfMethodConstraint(std::move(constraints))
                    .may_satisfy(method_mappings)
                    .is_bottom());
  }

  {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(
        std::make_unique<MethodNameConstraint>("method_name_a"));
    constraints.push_back(
        std::make_unique<MethodNameConstraint>("method_name_*"));

    EXPECT_EQ(
        AllOfMethodConstraint(std::move(constraints))
            .may_satisfy(method_mappings),
        marianatrench::MethodHashedSet({context.methods->get(method_a)}));
  }
}

TEST_F(MethodConstraintTest, AnyOfMethodConstraintMaySatisfy) {
  Scope scope;
  auto* method_a =
      redex::create_void_method(scope, "class_name", "method_name_a");
  auto* method_b =
      redex::create_void_method(scope, "class_name_b", "method_name_b");
  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  MethodMappings method_mappings{*context.methods};

  EXPECT_TRUE(AnyOfMethodConstraint({}).may_satisfy(method_mappings).is_top());

  {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(
        std::make_unique<MethodNameConstraint>("method_name_a"));

    EXPECT_EQ(
        AnyOfMethodConstraint(std::move(constraints))
            .may_satisfy(method_mappings),
        marianatrench::MethodHashedSet({context.methods->get(method_a)}));
  }

  {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(
        std::make_unique<MethodNameConstraint>("method_name_a"));
    constraints.push_back(
        std::make_unique<MethodNameConstraint>("method_name_b"));

    EXPECT_EQ(
        AnyOfMethodConstraint(std::move(constraints))
            .may_satisfy(method_mappings),
        marianatrench::MethodHashedSet(
            {context.methods->get(method_a), context.methods->get(method_b)}));
  }

  {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(
        std::make_unique<MethodNameConstraint>("method_name_nonexistant"));

    EXPECT_TRUE(AnyOfMethodConstraint(std::move(constraints))
                    .may_satisfy(method_mappings)
                    .is_bottom());
  }

  {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(
        std::make_unique<MethodNameConstraint>("method_name_a"));
    constraints.push_back(
        std::make_unique<MethodNameConstraint>("method_name_*"));

    EXPECT_TRUE(AnyOfMethodConstraint(std::move(constraints))
                    .may_satisfy(method_mappings)
                    .is_top());
  }
}

TEST_F(MethodConstraintTest, NotMethodConstraintMaySatisfy) {
  Scope scope;
  redex::create_void_method(scope, "class_name", "method_name_a");
  auto* method_b =
      redex::create_void_method(scope, "class_name_b", "method_name_b");
  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* array_allocation_method =
      context.artificial_methods->array_allocation_method();
  MethodMappings method_mappings{*context.methods};

  EXPECT_EQ(
      NotMethodConstraint(
          std::make_unique<MethodNameConstraint>("method_name_a"))
          .may_satisfy(method_mappings),
      marianatrench::MethodHashedSet(
          {context.methods->get(array_allocation_method),
           context.methods->get(method_b)}));

  EXPECT_TRUE(NotMethodConstraint(std::make_unique<MethodNameConstraint>(
                                      "method_name_nonexistant"))
                  .may_satisfy(method_mappings)
                  .is_top());

  EXPECT_TRUE(NotMethodConstraint(
                  std::make_unique<MethodNameConstraint>("method_name*"))
                  .may_satisfy(method_mappings)
                  .is_top());
}

TEST_F(MethodConstraintTest, UniqueConstraints) {
  Scope scope;
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  std::string class_name = "Landroid/test;";
  auto model_template = test::parse_json(R"({"sources": [{"kind": "Test"}]})");
  {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(std::make_unique<MethodNameConstraint>("test"));

    std::unordered_set<const MethodConstraint*> expected_constraints;
    for (const auto& constraint : constraints) {
      expected_constraints.insert(constraint.get());
    }

    const JsonModelGeneratorItem generator = JsonModelGeneratorItem(
        "test_generator_name",
        context,
        std::make_unique<AllOfMethodConstraint>(std::move(constraints)),
        ModelTemplate::from_json(model_template, context),
        0);
    EXPECT_EQ(generator.constraint_leaves(), expected_constraints);
  }

  {
    // Constraint components
    auto constraint_a = std::make_unique<MethodNameConstraint>("test_a");
    auto constraint_b = std::make_unique<MethodNameConstraint>("test_b");
    auto constraint_c = std::make_unique<MethodNameConstraint>("test_c");
    auto constraint_d = std::make_unique<ParentConstraint>(
        std::make_unique<TypeNameConstraint>("test_d"));

    std::unordered_set<const MethodConstraint*> expected_constraints = {
        constraint_a.get(),
        constraint_b.get(),
        constraint_c.get(),
        constraint_d.get()};

    // Aggregate and nested constraints
    std::vector<std::unique_ptr<MethodConstraint>> all_constraint_elements;
    all_constraint_elements.push_back(std::move(constraint_a));
    all_constraint_elements.push_back(std::move(constraint_b));
    auto all_constraint = std::make_unique<AllOfMethodConstraint>(
        std::move(all_constraint_elements));
    auto not_constraint =
        std::make_unique<NotMethodConstraint>(std::move(constraint_c));
    std::vector<std::unique_ptr<MethodConstraint>> any_constraint_elements;
    any_constraint_elements.push_back(std::move(constraint_d));
    any_constraint_elements.push_back(std::move(all_constraint));
    any_constraint_elements.push_back(std::move(not_constraint));
    auto any_constraint = std::make_unique<AnyOfMethodConstraint>(
        std::move(any_constraint_elements));
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    constraints.push_back(std::move(any_constraint));

    const JsonModelGeneratorItem generator = JsonModelGeneratorItem(
        "test_generator_name",
        context,
        std::make_unique<AllOfMethodConstraint>(std::move(constraints)),
        ModelTemplate::from_json(model_template, context),
        0);
    EXPECT_EQ(generator.constraint_leaves(), expected_constraints);
  }
}

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/LifecycleMethod.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

class LifecycleMethodTest : public test::Test {};

std::unique_ptr<ClassHierarchies> make_class_hierarchies(const Scope& scope) {
  DexStore store("test_store");
  store.add_classes(scope);
  DexStoresVector stores{store};
  auto options = test::make_default_options();
  return std::make_unique<ClassHierarchies>(
      *options, options->analysis_mode(), stores);
}

struct DerivedClasses {
  DexClass* base_class;
  DexClass* derived_class;
  std::unique_ptr<ClassHierarchies> class_hierarchies;
};

DerivedClasses make_derived_classes(Scope& scope) {
  auto* base_class = marianatrench::redex::create_class(scope, "LBase;");
  auto* derived_class = marianatrench::redex::create_class(
      scope, "LDerived;", base_class->get_type());
  return {base_class, derived_class, make_class_hierarchies(scope)};
}

TEST_F(LifecycleMethodTest, AllowsDcePrunedDerivedMethod) {
  Scope scope;
  auto classes = make_derived_classes(scope);

  LifecycleMethodCall call(
      "dcePrunedMethod",
      "V",
      {},
      /* defined_in_derived_class */ "LDerived;",
      /* skip_base_implementation */ false);

  EXPECT_EQ(call.get_dex_method(classes.derived_class), nullptr);
  EXPECT_NO_THROW(
      call.validate(classes.base_class, *classes.class_hierarchies));
}

TEST_F(LifecycleMethodTest, AllowsDcePrunedDerivedMethodWithPrunedTypes) {
  Scope scope;
  auto classes = make_derived_classes(scope);

  LifecycleMethodCall call(
      "dcePrunedMethod",
      "LPrunedReturnType;",
      {"LPrunedArgumentType;"},
      /* defined_in_derived_class */ "LDerived;",
      /* skip_base_implementation */ false);

  EXPECT_NO_THROW(
      call.validate(classes.base_class, *classes.class_hierarchies));
}

TEST_F(LifecycleMethodTest, RejectsMalformedBaseClassReturnType) {
  Scope scope;
  auto* base_class = marianatrench::redex::create_class(scope, "LBase;");
  auto class_hierarchies = make_class_hierarchies(scope);

  LifecycleMethodCall call(
      "method",
      "not-a-type",
      {},
      /* defined_in_derived_class */ std::nullopt,
      /* skip_base_implementation */ false);

  try {
    call.validate(base_class, *class_hierarchies);
    FAIL() << "Expected lifecycle validation to reject a malformed return type";
  } catch (const LifecycleMethodValidationError& error) {
    EXPECT_STREQ(
        error.what(),
        "Callee `method()not-a-type` has a malformed return type.");
  }
}

TEST_F(LifecycleMethodTest, RejectsUnrelatedDerivedClass) {
  Scope scope;
  auto* base_class = marianatrench::redex::create_class(scope, "LBase;");
  marianatrench::redex::create_class(scope, "LUnrelated;");

  auto class_hierarchies = make_class_hierarchies(scope);

  LifecycleMethodCall call(
      "method",
      "V",
      {},
      /* defined_in_derived_class */ "LUnrelated;",
      /* skip_base_implementation */ false);

  try {
    call.validate(base_class, *class_hierarchies);
    FAIL() << "Expected lifecycle validation to reject an unrelated class";
  } catch (const LifecycleMethodValidationError& error) {
    EXPECT_STREQ(
        error.what(),
        "Derived class `LUnrelated;` is not derived from base class `LBase;`.");
  }
}

TEST_F(LifecycleMethodTest, RejectsMalformedDerivedClassType) {
  Scope scope;
  auto classes = make_derived_classes(scope);

  LifecycleMethodCall call(
      "method",
      "V",
      {},
      /* defined_in_derived_class */ "not-a-type",
      /* skip_base_implementation */ false);

  try {
    call.validate(classes.base_class, *classes.class_hierarchies);
    FAIL() << "Expected lifecycle validation to reject a malformed derived "
              "class type";
  } catch (const LifecycleMethodValidationError& error) {
    EXPECT_STREQ(error.what(), "Derived class type `not-a-type` is malformed.");
  }
}

TEST_F(LifecycleMethodTest, RejectsNonClassDerivedClassTypes) {
  Scope scope;
  auto classes = make_derived_classes(scope);

  for (const auto* derived_type : {"I", "[LDerived;"}) {
    LifecycleMethodCall call(
        "method",
        "V",
        {},
        /* defined_in_derived_class */ derived_type,
        /* skip_base_implementation */ false);

    try {
      call.validate(classes.base_class, *classes.class_hierarchies);
      FAIL() << "Expected lifecycle validation to reject non-class derived "
                "type "
             << derived_type;
    } catch (const LifecycleMethodValidationError& error) {
      const auto expected = std::string("Derived class type `") + derived_type +
          "` is malformed.";
      EXPECT_STREQ(error.what(), expected.c_str());
    }
  }
}

TEST_F(LifecycleMethodTest, RejectsMalformedArgumentTypes) {
  Scope scope;
  auto classes = make_derived_classes(scope);

  LifecycleMethodCall call(
      "method",
      "V",
      {"not-a-type"},
      /* defined_in_derived_class */ "LDerived;",
      /* skip_base_implementation */ false);

  try {
    call.validate(classes.base_class, *classes.class_hierarchies);
    FAIL()
        << "Expected lifecycle validation to reject a malformed argument type";
  } catch (const LifecycleMethodValidationError& error) {
    EXPECT_STREQ(
        error.what(),
        "Callee `method(not-a-type)V` has malformed argument types.");
  }
}

TEST_F(LifecycleMethodTest, RejectsVoidArgumentType) {
  Scope scope;
  auto classes = make_derived_classes(scope);

  LifecycleMethodCall call(
      "method",
      "V",
      {"V"},
      /* defined_in_derived_class */ "LDerived;",
      /* skip_base_implementation */ false);

  try {
    call.validate(classes.base_class, *classes.class_hierarchies);
    FAIL() << "Expected lifecycle validation to reject a void argument type";
  } catch (const LifecycleMethodValidationError& error) {
    EXPECT_STREQ(
        error.what(), "Callee `method(V)V` has malformed argument types.");
  }
}

TEST_F(LifecycleMethodTest, RejectsMalformedReturnType) {
  Scope scope;
  auto classes = make_derived_classes(scope);

  LifecycleMethodCall call(
      "method",
      "not-a-type",
      {},
      /* defined_in_derived_class */ "LDerived;",
      /* skip_base_implementation */ false);

  try {
    call.validate(classes.base_class, *classes.class_hierarchies);
    FAIL() << "Expected lifecycle validation to reject a malformed return type";
  } catch (const LifecycleMethodValidationError& error) {
    EXPECT_STREQ(
        error.what(),
        "Callee `method()not-a-type` has a malformed return type.");
  }
}

TEST_F(LifecycleMethodTest, RejectsArrayOfVoidReturnType) {
  Scope scope;
  auto classes = make_derived_classes(scope);

  LifecycleMethodCall call(
      "method",
      "[V",
      {},
      /* defined_in_derived_class */ "LDerived;",
      /* skip_base_implementation */ false);

  try {
    call.validate(classes.base_class, *classes.class_hierarchies);
    FAIL() << "Expected lifecycle validation to reject an array-of-void return "
              "type";
  } catch (const LifecycleMethodValidationError& error) {
    EXPECT_STREQ(
        error.what(), "Callee `method()[V` has a malformed return type.");
  }
}

} // namespace

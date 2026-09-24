/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/ReturnsThisAnalyzer.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {
class ReturnsThisAnalyzerTest : public test::Test {};

struct SingleMoveTestCase final {
  const char* name;
  const char* method_body;
};

class ReturnsThisAnalyzerTestSingleMove
    : public ReturnsThisAnalyzerTest,
      public testing::WithParamInterface<SingleMoveTestCase> {};

std::vector<const Method*> get_methods(
    Context& context,
    std::vector<DexMethod*> dex_methods) {
  // ReturnsThis requires the cfg to be built.
  for (auto* method : dex_methods) {
    method->get_code()->build_cfg();
  }

  std::vector<const Method*> methods;
  std::transform(
      dex_methods.begin(),
      dex_methods.end(),
      std::back_inserter(methods),
      [&context](DexMethod* dex_method) -> const Method* {
        return context.methods->create(dex_method);
      });

  return methods;
}

bool returns_this(const std::string& method_body) {
  auto context = test::make_empty_context();
  Scope scope;
  auto dex_methods =
      marianatrench::redex::create_methods(scope, "LClass;", {method_body});
  auto methods = get_methods(context, dex_methods);

  return returns_this_analyzer::method_returns_this(methods.front());
}

} // namespace

TEST_F(ReturnsThisAnalyzerTest, ReturnsThisConstraint) {
  auto context = test::make_empty_context();
  Scope scope;
  auto dex_methods = marianatrench::redex::create_methods(
      scope,
      "LClass;",
      {
          R"(
            (method (public) "LClass;.method_1:()LClass;"
            (
              (load-param-object v1)
              (return-object v1)
            )
            ))",
          R"(
            (method (public) "LClass;.method_2:(Z)Z;"
            (
              (load-param v1)
              (return v1)
            )
            ))",
          R"(
            (method (public) "LClass;.method_3:(Z)LClass;"
            (
              (load-param-object v1)
              (load-param v2)
              (new-instance "LClass;")
              (move-result-pseudo-object v0)
              (return-object v0)
            )
            ))",
      });

  auto methods = get_methods(context, dex_methods);

  EXPECT_TRUE(returns_this_analyzer::method_returns_this(methods[0]));
  EXPECT_FALSE(returns_this_analyzer::method_returns_this(methods[1]));
  EXPECT_FALSE(returns_this_analyzer::method_returns_this(methods[2]));
}

TEST_F(ReturnsThisAnalyzerTest, MultipleReturns) {
  auto context = test::make_empty_context();
  Scope scope;
  auto dex_methods = marianatrench::redex::create_methods(
      scope,
      "LClass;",
      {
          R"(
            (method (public) "LClass;.maybe_new_instance:(Z)LClass;"
            (
              (load-param-object v1)
              (load-param v2)
              (if-nez v2 :L0)
              (new-instance "LClass;")
              (move-result-pseudo-object v0)
              (return-object v0)
              (:L0)
              (return-object v1)
            )
            ))",
      });
  DexStore store("stores");
  store.add_classes(scope);
  auto methods = get_methods(context, dex_methods);

  EXPECT_TRUE(returns_this_analyzer::method_returns_this(methods[0]));
}

// Redex represents all three DEX object-move encodings as
// OPCODE_MOVE_OBJECT. These register shapes select the corresponding encoding
// when the IR is lowered.
TEST_P(ReturnsThisAnalyzerTestSingleMove, PreservesThisParameter) {
  EXPECT_TRUE(returns_this(GetParam().method_body));
}

INSTANTIATE_TEST_SUITE_P(
    MoveObjectEncoding,
    ReturnsThisAnalyzerTestSingleMove,
    testing::Values(
        SingleMoveTestCase{
            "MoveObject",
            R"(
              (method (public) "LClass;.move_object:()LClass;"
              (
                (load-param-object v1)
                (move-object v0 v1)
                (return-object v0)
              )
              ))"},
        SingleMoveTestCase{
            "MoveObjectFrom16",
            R"(
              (method (public) "LClass;.move_object_from16:()LClass;"
              (
                (load-param-object v17)
                (move-object v2 v17)
                (return-object v2)
              )
              ))"}),
    [](const testing::TestParamInfo<SingleMoveTestCase>& info) {
      return info.param.name;
    });

TEST_F(ReturnsThisAnalyzerTest, MoveObject16PreservesThisParameter) {
  EXPECT_TRUE(returns_this(R"(
    (method (public) "LClass;.move_object_16:()LClass;"
    (
      (load-param-object v17)
      (move-object v300 v17)
      (move-object v2 v300)
      (return-object v2)
    )
    ))"));
}

TEST_F(ReturnsThisAnalyzerTest, MoveObjectDoesNotCreateThisParameter) {
  EXPECT_FALSE(returns_this(R"(
    (method (public) "LClass;.move_other_object:(LClass;)LClass;"
    (
      (load-param-object v1)
      (load-param-object v17)
      (move-object v2 v17)
      (return-object v2)
    )
    ))"));
}

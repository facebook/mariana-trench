/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/BuilderPatternGenerator.h>
#include <mariana-trench/model-generator/ReturnsThisAnalyzer.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {
class BuilderPatternGeneratorTest : public test::Test {};

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

} // namespace

TEST_F(BuilderPatternGeneratorTest, BuilderPatternModels) {
  Scope scope;
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto dex_methods = redex::create_methods(
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

  auto builder_pattern_generator = BuilderPatternGenerator(context);
  auto builder_pattern_models =
      builder_pattern_generator.visit_method(methods[0]);

  EXPECT_EQ(builder_pattern_models.size(), 1);
  EXPECT_THAT(
      builder_pattern_models,
      testing::UnorderedElementsAre(Model(
          methods[0], context, Model::Mode::AliasMemoryLocationOnInvoke)));

  builder_pattern_models = builder_pattern_generator.visit_method(methods[1]);
  EXPECT_EQ(builder_pattern_models.size(), 0);

  builder_pattern_models = builder_pattern_generator.visit_method(methods[2]);

  EXPECT_EQ(builder_pattern_models.size(), 0);
}

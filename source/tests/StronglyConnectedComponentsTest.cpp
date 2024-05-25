/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/Dependencies.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/StronglyConnectedComponents.h>
#include <mariana-trench/shim-generator/ShimGenerator.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

class StronglyConnectedComponentsTest : public test::Test {};

Context test_components(const Scope& scope) {
  Context context;
  context.options = std::make_unique<Options>(
      /* models_path */ std::vector<std::string>{},
      /* field_models_path */ std::vector<std::string>{},
      /* literal_models_path */ std::vector<std::string>{},
      /* rules_path */ std::vector<std::string>{},
      /* lifecycles_path */ std::vector<std::string>{},
      /* shims_path */ std::vector<std::string>{},
      /* graphql_metadata_paths */ std::string{},
      /* proguard_configuration_paths */ std::vector<std::string>{},
      /* sequential */ false,
      /* skip_source_indexing */ true,
      /* skip_analysis */ true,
      /* model_generators_configuration */
      std::vector<ModelGeneratorConfiguration>{},
      /* model_generator_search_paths */ std::vector<std::string>{},
      /* remove_unreachable_code */ false,
      /* emit_all_via_cast_features */ false);
  CachedModelsContext cached_models_context(context, *context.options);
  DexStore store("test_store");
  store.add_classes(scope);
  context.stores = {store};
  context.artificial_methods = std::make_unique<ArtificialMethods>(
      *context.kind_factory, context.stores);
  context.methods = std::make_unique<Methods>(context.stores);
  MethodMappings method_mappings{*context.methods};
  context.control_flow_graphs =
      std::make_unique<ControlFlowGraphs>(context.stores);
  context.types = std::make_unique<Types>(*context.options, context.stores);
  context.class_hierarchies = std::make_unique<ClassHierarchies>(
      *context.options, context.stores, cached_models_context);
  context.overrides = std::make_unique<Overrides>(
      *context.options,
      *context.methods,
      context.stores,
      cached_models_context);
  context.fields = std::make_unique<Fields>();
  context.call_graph = std::make_unique<CallGraph>(
      *context.options,
      *context.types,
      *context.class_hierarchies,
      LifecycleMethods{},
      Shims{/* global_shims_size */ 0},
      *context.feature_factory,
      *context.methods,
      *context.fields,
      *context.overrides,
      method_mappings);
  context.rules = std::make_unique<Rules>(context);
  auto registry = Registry(context);
  context.dependencies = std::make_unique<Dependencies>(
      *context.options,
      *context.methods,
      *context.overrides,
      *context.call_graph,
      registry);
  return context;
}

void filter_components(
    std::vector<std::vector<const Method*>>& components,
    const std::unordered_set<const Method*>& keep) {
  for (auto& component : components) {
    component.erase(
        std::remove_if(
            component.begin(),
            component.end(),
            [&](const auto* method) { return keep.count(method) == 0; }),
        component.end());
  }

  components.erase(
      std::remove_if(
          components.begin(),
          components.end(),
          [](const auto& component) { return component.empty(); }),
      components.end());
}

} // anonymous namespace

TEST_F(StronglyConnectedComponentsTest, DiamondGraph) {
  Scope scope;

  /*
   *    Top
   *  /     \
   * Left  Right
   *   \    /
   *   Bottom
   */
  auto* dex_bottom = redex::create_void_method(scope, "LBottom;", "bottom");
  auto* dex_left = redex::create_method(scope, "LLeft;", R"(
    (method (public) "LLeft;.left:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LBottom;.bottom:()V")
      (return-void)
     )
    )
  )");
  auto* dex_right = redex::create_method(scope, "LRight;", R"(
    (method (public) "LRight;.right:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LBottom;.bottom:()V")
      (return-void)
     )
    )
  )");
  auto* dex_top = redex::create_method(scope, "LTop;", R"(
    (method (public) "LTop;.top:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LLeft;.left:()V")
      (invoke-direct (v0) "LRight;.right:()V")
      (return-void)
     )
    )
  )");

  auto context = test_components(scope);
  auto* bottom = context.methods->get(dex_bottom);
  auto* top = context.methods->get(dex_top);
  auto* left = context.methods->get(dex_left);
  auto* right = context.methods->get(dex_right);

  std::vector<std::vector<const Method*>> components =
      StronglyConnectedComponents(*context.methods, *context.dependencies)
          .components();
  filter_components(components, /* keep */ {top, bottom, left, right});

  EXPECT_EQ(components.size(), 4);
  EXPECT_THAT(components[0], testing::ElementsAre(bottom));
  if (components[1].at(0) == left) {
    EXPECT_THAT(components[1], testing::ElementsAre(left));
    EXPECT_THAT(components[2], testing::ElementsAre(right));
  } else {
    EXPECT_THAT(components[2], testing::ElementsAre(left));
    EXPECT_THAT(components[1], testing::ElementsAre(right));
  }
  EXPECT_THAT(components[3], testing::ElementsAre(top));
}

TEST_F(StronglyConnectedComponentsTest, Recursive) {
  Scope scope;

  /*
   *    Top
   *  /     \
   * Left - Right
   *   \    /
   *   Bottom
   */
  auto* dex_bottom = redex::create_void_method(scope, "LBottom;", "bottom");
  auto* dex_left = redex::create_method(scope, "LLeft;", R"(
    (method (public) "LLeft;.left:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LBottom;.bottom:()V")
      (invoke-direct (v0) "LRight;.right:()V")
      (return-void)
     )
    )
  )");
  auto* dex_right = redex::create_method(scope, "LRight;", R"(
    (method (public) "LRight;.right:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LBottom;.bottom:()V")
      (invoke-direct (v0) "LTop;.top:()V")
      (return-void)
     )
    )
  )");
  auto* dex_top = redex::create_method(scope, "LTop;", R"(
    (method (public) "LTop;.top:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LLeft;.left:()V")
      (return-void)
     )
    )
  )");

  auto context = test_components(scope);
  auto* bottom = context.methods->get(dex_bottom);
  auto* top = context.methods->get(dex_top);
  auto* left = context.methods->get(dex_left);
  auto* right = context.methods->get(dex_right);

  std::vector<std::vector<const Method*>> components =
      StronglyConnectedComponents(*context.methods, *context.dependencies)
          .components();
  filter_components(components, /* keep */ {top, bottom, left, right});

  EXPECT_EQ(components.size(), 2);
  EXPECT_THAT(components[0], testing::ElementsAre(bottom));
  EXPECT_THAT(components[1], testing::UnorderedElementsAre(top, left, right));
}

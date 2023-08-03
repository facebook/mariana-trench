/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/Overrides.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

class OverridesTest : public test::Test {};

Context test_overrides(const Scope& scope) {
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
  DexStore store("test_store");
  store.add_classes(scope);
  context.stores = {store};
  context.artificial_methods = std::make_unique<ArtificialMethods>(
      *context.kind_factory, context.stores);
  context.methods = std::make_unique<Methods>(context.stores);
  context.control_flow_graphs =
      std::make_unique<ControlFlowGraphs>(context.stores);
  context.types = std::make_unique<Types>(*context.options, context.stores);
  context.class_hierarchies =
      std::make_unique<ClassHierarchies>(*context.options, context.stores);
  context.overrides = std::make_unique<Overrides>(
      *context.options, *context.methods, context.stores);
  return context;
}

} // anonymous namespace

TEST_F(OverridesTest, Overrides) {
  Scope scope;

  auto* dex_callee = redex::create_void_method(scope, "LCallee;", "callee");
  auto* dex_override_one = redex::create_void_method(
      scope,
      "LSubclassOne;",
      "callee",
      /* parameter_types */ "",
      /* return_type */ "V",
      /* super */ dex_callee->get_class());
  auto* dex_override_two = redex::create_void_method(
      scope,
      "LSubclassTwo;",
      "callee",
      /* parameter_types */ "",
      /* return_type */ "V",
      /* super */ dex_callee->get_class());
  auto* dex_indirect_override = redex::create_void_method(
      scope,
      "LIndirectSubclass;",
      "callee",
      /* parameter_types */ "",
      /* return_type */ "V",
      /* super */ dex_override_two->get_class());

  auto context = test_overrides(scope);
  const auto& overrides = *context.overrides;
  auto* callee = context.methods->get(dex_callee);
  auto* override_one = context.methods->get(dex_override_one);
  auto* override_two = context.methods->get(dex_override_two);
  auto* indirect_override = context.methods->get(dex_indirect_override);

  EXPECT_THAT(
      overrides.get(callee),
      testing::UnorderedElementsAre(
          override_one, override_two, indirect_override));
  EXPECT_TRUE(overrides.get(override_one).empty());
  EXPECT_THAT(
      overrides.get(override_two),
      testing::UnorderedElementsAre(indirect_override));
  EXPECT_TRUE(overrides.get(indirect_override).empty());
}

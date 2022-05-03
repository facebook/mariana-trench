/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

class ClassHierarchiesTest : public test::Test {};

Context test_class_hierarchies(const Scope& scope) {
  Context context;
  context.options = std::make_unique<Options>(
      /* models_path */ std::vector<std::string>{},
      /* field_models_path */ std::vector<std::string>{},
      /* rules_path */ std::vector<std::string>{},
      /* lifecycles_path */ std::vector<std::string>{},
      /* shims_path */ std::vector<std::string>{},
      /* proguard_configuration_paths */ std::vector<std::string>{},
      /* sequential */ false,
      /* skip_source_indexing */ true,
      /* skip_model_generation */ true,
      /* model_generators_configuration */
      std::vector<ModelGeneratorConfiguration>{},
      /* model_generator_search_paths */ std::vector<std::string>{},
      /* remove_unreachable_code */ false);
  DexStore store("test_store");
  store.add_classes(scope);
  context.stores = {store};
  context.artificial_methods =
      std::make_unique<ArtificialMethods>(*context.kinds, context.stores);
  context.methods = std::make_unique<Methods>(context.stores);
  context.types = std::make_unique<Types>(*context.options, context.stores);
  context.class_hierarchies =
      std::make_unique<ClassHierarchies>(*context.options, context.stores);
  return context;
}

} // anonymous namespace

TEST_F(ClassHierarchiesTest, ClassHierarchies) {
  Scope scope;

  auto* dex_parent = redex::create_void_method(scope, "LParent;", "f");
  auto* dex_child_one = redex::create_void_method(
      scope,
      "LChildOne;",
      "f",
      /* parameter_types */ "",
      /* return_type */ "V",
      /* super */ dex_parent->get_class());
  auto* dex_child_two = redex::create_void_method(
      scope,
      "LChildTwo;",
      "f",
      /* parameter_types */ "",
      /* return_type */ "V",
      /* super */ dex_parent->get_class());
  auto* dex_child_one_child = redex::create_void_method(
      scope,
      "LChildOneChild;",
      "f",
      /* parameter_types */ "",
      /* return_type */ "V",
      /* super */ dex_child_one->get_class());

  auto context = test_class_hierarchies(scope);
  const auto& class_hierarchies = *context.class_hierarchies;

  EXPECT_THAT(
      class_hierarchies.extends(dex_parent->get_class()),
      testing::UnorderedElementsAre(
          dex_child_one->get_class(),
          dex_child_two->get_class(),
          dex_child_one_child->get_class()));
  EXPECT_THAT(
      class_hierarchies.extends(dex_child_one->get_class()),
      testing::UnorderedElementsAre(dex_child_one_child->get_class()));
  EXPECT_TRUE(class_hierarchies.extends(dex_child_two->get_class()).empty());
  EXPECT_TRUE(
      class_hierarchies.extends(dex_child_one_child->get_class()).empty());
}

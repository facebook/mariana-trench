/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/FieldCache.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

class FieldCacheTest : public test::Test {};

Context test_fields(const Scope& scope) {
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
      /* remove_unreachable_code */ false,
      /* emit_all_via_cast_features */ false);
  DexStore store("test_store");
  store.add_classes(scope);
  context.stores = {store};
  context.artificial_methods =
      std::make_unique<ArtificialMethods>(*context.kinds, context.stores);
  context.methods = std::make_unique<Methods>(context.stores);
  context.types = std::make_unique<Types>(*context.options, context.stores);
  context.class_hierarchies =
      std::make_unique<ClassHierarchies>(*context.options, context.stores);
  context.field_cache =
      std::make_unique<FieldCache>(*context.class_hierarchies, context.stores);
  return context;
}

} // anonymous namespace

TEST_F(FieldCacheTest, FieldCache) {
  Scope scope;

  redex::create_fields(
      scope,
      /* class_name */ "LBase;",
      /* fields */
      {{"mBase", type::java_lang_String()}});
  redex::create_fields(
      scope,
      /* class_name */ "LDerived;",
      /* fields */
      {{"mDerived", type::java_lang_String()},
       {"mBase", redex::get_type("LBase;")}},
      /* super */ redex::get_type("LBase;"));

  auto context = test_fields(scope);
  const auto& field_cache = *context.field_cache;

  EXPECT_THAT(
      field_cache.field_types(
          redex::get_type("LBase;"), DexString::make_string("mBase")),
      testing::UnorderedElementsAre(
          type::java_lang_String(), redex::get_type("LBase;")));
  EXPECT_TRUE(field_cache
                  .field_types(
                      redex::get_type("LBase;"),
                      DexString::make_string("mFieldDoesNotExist"))
                  .empty());
  EXPECT_THAT(
      field_cache.field_types(
          redex::get_type("LDerived;"), DexString::make_string("mDerived")),
      testing::UnorderedElementsAre(type::java_lang_String()));
  EXPECT_THAT(
      field_cache.field_types(
          redex::get_type("LDerived;"), DexString::make_string("mBase")),
      testing::UnorderedElementsAre(
          type::java_lang_String(), redex::get_type("LBase;")));
  EXPECT_TRUE(field_cache
                  .field_types(
                      redex::get_type("LClassDoesNotExist;"),
                      DexString::make_string("mSomething"))
                  .empty());
}

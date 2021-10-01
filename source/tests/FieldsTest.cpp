/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/Fields.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

class FieldsTest : public test::Test {};

Context test_fields(const Scope& scope) {
  Context context;
  context.options = std::make_unique<Options>(
      /* models_path */ std::vector<std::string>{},
      /* rules_path */ std::vector<std::string>{},
      /* lifecycles_path */ std::vector<std::string>{},
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
  context.fields =
      std::make_unique<Fields>(*context.class_hierarchies, context.stores);
  return context;
}

} // anonymous namespace

TEST_F(FieldsTest, Fields) {
  Scope scope;

  redex::create_fields(
      scope,
      /* class_name */ "LBase;",
      /* fields */
      std::vector{std::make_pair<std::string, const DexType*>(
          "mBase", type::java_lang_String())});
  redex::create_fields(
      scope,
      /* class_name */ "LDerived;",
      /* fields */
      std::vector{
          std::make_pair<std::string, const DexType*>(
              "mDerived", type::java_lang_String()),
          std::make_pair<std::string, const DexType*>(
              "mBase", redex::get_type("LBase;"))},
      /* super */ redex::get_type("LBase;"));

  auto context = test_fields(scope);
  const auto& fields = *context.fields;

  EXPECT_THAT(
      fields.field_types(
          redex::get_type("LBase;"), DexString::make_string("mBase")),
      testing::UnorderedElementsAre(
          type::java_lang_String(), redex::get_type("LBase;")));
  EXPECT_TRUE(fields
                  .field_types(
                      redex::get_type("LBase;"),
                      DexString::make_string("mFieldDoesNotExist"))
                  .empty());
  EXPECT_THAT(
      fields.field_types(
          redex::get_type("LDerived;"), DexString::make_string("mDerived")),
      testing::UnorderedElementsAre(type::java_lang_String()));
  EXPECT_THAT(
      fields.field_types(
          redex::get_type("LDerived;"), DexString::make_string("mBase")),
      testing::UnorderedElementsAre(
          type::java_lang_String(), redex::get_type("LBase;")));
  EXPECT_TRUE(fields
                  .field_types(
                      redex::get_type("LClassDoesNotExist;"),
                      DexString::make_string("mSomething"))
                  .empty());
}

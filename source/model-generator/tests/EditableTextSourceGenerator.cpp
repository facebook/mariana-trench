/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/filesystem.hpp>
#include <gtest/gtest.h>

#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/JsonModelGenerator.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

const auto json_file_path = boost::filesystem::current_path() /
    "fbandroid/native/mariana-trench/shim/resources/model_generators/sources/EditableTextSourceGenerator.json";
class EditableTextSourceGeneratorTest : public test::Test {};

} // namespace

TEST_F(EditableTextSourceGeneratorTest, OverrideSourceMethod) {
  Scope scope;

  auto* dex_base_method = redex::create_void_method(
      scope,
      /* class_name */ "Landroid/widget/EditText;",
      /* method */ "getText",
      /* parameter_type */ "",
      /* return_type */ "Landroid/text/Editable;");

  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "Lcom/facebook/xyz/IGEditText;",
      /* method */ "getText",
      /* parameter_type */ "",
      /* return_type */ "Landroid/text/Editable;",
      /* super */ dex_base_method->get_class());

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* base_method = context.methods->get(dex_base_method);
  auto* method = context.methods->get(dex_method);

  EXPECT_THAT(
      JsonModelGenerator("EditableTextSourceGenerator", context, json_file_path)
          .run(context.stores),
      testing::UnorderedElementsAre(
          Model(
              /* method */ base_method,
              context,
              /* modes */ Model::Mode::Normal,
              /* generations */
              {{AccessPath(Root(Root::Kind::Return)),
                generator::source(
                    context,
                    /* method */ base_method,
                    /* kind */ "EditableText")}}),

          Model(
              /* method */ method,
              context,
              /* modes */ Model::Mode::Normal,
              /* generations */ {},
              /* parameter_sources */
              {{AccessPath(Root(Root::Kind::Argument, 0)),
                generator::source(
                    context,
                    /* method */ method,
                    /* kind */ "EditableText")}})));
}

TEST_F(EditableTextSourceGeneratorTest, NoOverrideMethod) {
  Scope scope;

  redex::create_void_method(
      scope,
      /* class_name */ "Lcom/facebook/xyz/IGEditText;",
      /* method */ "getText",
      /* parameter_type */ "",
      /* return_type */ "Landroid/text/Editable;");

  DexStore store("test-stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THAT(
      JsonModelGenerator("EditableTextSourceGenerator", context, json_file_path)
          .run(context.stores),
      testing::UnorderedElementsAre());
}

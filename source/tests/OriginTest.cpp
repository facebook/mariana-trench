/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>
#include <gmock/gmock.h>

#include <mariana-trench/Origin.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class OriginTest : public test::Test {};

TEST_F(OriginTest, CallInfoSerializationDeserialization) {
  Scope scope;

  // Field needs to be part of scope to be accessible using
  // context.fields->get(DexField*)
  const auto* dex_field = redex::create_field(
      scope,
      "LClassWithField;",
      {.field_name = "mField", .field_type = type::java_lang_String()});

  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  {
    const auto* method = context.methods->create(
        redex::create_void_method(scope, "LClass;", "one"));
    const auto* port = context.access_path_factory->get(
        AccessPath(Root(Root::Kind::Argument, 0)));
    const auto* origin = context.origin_factory->method_origin(method, port);
    EXPECT_EQ(Origin::from_json(origin->to_json(), context), origin);
  }

  {
    const auto* field = context.fields->get(dex_field);
    const auto* origin = context.origin_factory->field_origin(field);
    EXPECT_EQ(Origin::from_json(origin->to_json(), context), origin);
  }

  {
    const auto* port = context.access_path_factory->get(
        AccessPath(Root(Root::Kind::Argument, 0)));
    const auto* origin =
        context.origin_factory->crtex_origin("canonical name", port);
    EXPECT_EQ(Origin::from_json(origin->to_json(), context), origin);
  }

  {
    const auto* origin = context.origin_factory->string_origin("string");
    EXPECT_EQ(Origin::from_json(origin->to_json(), context), origin);
  }
}

} // namespace marianatrench

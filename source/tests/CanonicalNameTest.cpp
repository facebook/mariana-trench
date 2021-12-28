/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/CanonicalName.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class CanonicalNameTest : public test::Test {};

TEST_F(CanonicalNameTest, Instantiate) {
  Scope scope;
  auto context = test::make_empty_context();
  const auto* method = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));
  const auto* feature1 = context.features->get("feature1");
  const auto* feature2 = context.features->get("feature2");

  EXPECT_EQ(
      CanonicalName(CanonicalName::TemplateValue{"%programmatic_leaf_name%"})
          .instantiate(method, /* via_type_ofs */ {})
          .value(),
      CanonicalName(CanonicalName::InstantiatedValue{"LClass;.one:()V"}));

  EXPECT_EQ(
      CanonicalName(CanonicalName::TemplateValue{
                        "%programmatic_leaf_name%__%programmatic_leaf_name%"})
          .instantiate(method, /* via_type_ofs */ {})
          .value(),
      CanonicalName(CanonicalName::InstantiatedValue{
          "LClass;.one:()V__LClass;.one:()V"}));

  EXPECT_EQ(
      CanonicalName(CanonicalName::TemplateValue{
                        "%programmatic_leaf_name%__%via_type_of%"})
          .instantiate(method, /* via_type_ofs */ {feature1})
          .value(),
      CanonicalName(
          CanonicalName::InstantiatedValue{"LClass;.one:()V__feature1"}));

  EXPECT_EQ(
      CanonicalName(CanonicalName::TemplateValue{"static name"})
          .instantiate(method, /* via_type_ofs */ {})
          .value(),
      CanonicalName(CanonicalName::InstantiatedValue{"static name"}));

  EXPECT_EQ(
      CanonicalName(CanonicalName::TemplateValue{"%via_type_of%"})
          .instantiate(method, /* via_type_ofs */ {}),
      std::nullopt);

  EXPECT_EQ(
      CanonicalName(CanonicalName::TemplateValue{"%programmatic_leaf_name%"})
          .instantiate(method, /* via_type_ofs */ {feature1, feature2})
          .value(),
      CanonicalName(CanonicalName::InstantiatedValue{"LClass;.one:()V"}));
}

} // namespace marianatrench

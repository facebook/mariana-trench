/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <DexStore.h>

#include <mariana-trench/FieldSet.h>
#include <mariana-trench/Fields.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class FieldSetTest : public test::Test {
 public:
  const Field* field_a;
  const Field* field_b;
  const Field* field_c;

  FieldSetTest() {
    Scope scope;
    const auto* dex_field_a = redex::create_field(
        scope, "LClassA", {"field_a", type::java_lang_String()});
    const auto* dex_field_b = redex::create_field(
        scope, "LClassB", {"field_b", type::java_lang_String()});
    const auto* dex_field_c = redex::create_field(
        scope, "LClassC", {"field_c", type::java_lang_String()});
    DexStore store("stores");
    store.add_classes(scope);
    auto context = test::make_context(store);
    field_a = context.fields->get(dex_field_a);
    field_b = context.fields->get(dex_field_b);
    field_c = context.fields->get(dex_field_c);
  }
};

TEST_F(FieldSetTest, Constructor) {
  EXPECT_TRUE(FieldSet().is_bottom());
  EXPECT_TRUE(FieldSet().empty());
  EXPECT_FALSE(FieldSet().is_top());

  EXPECT_TRUE(FieldSet({}).is_bottom());
  EXPECT_FALSE(FieldSet({field_a}).is_bottom());
}

TEST_F(FieldSetTest, SetTopBottom) {
  auto fields = FieldSet({});
  EXPECT_FALSE(fields.is_top());
  EXPECT_TRUE(fields.is_bottom());
  fields.set_to_top();
  EXPECT_TRUE(fields.is_top());
  EXPECT_FALSE(fields.is_bottom());
  fields.set_to_bottom();
  EXPECT_FALSE(fields.is_top());
  EXPECT_TRUE(fields.is_bottom());
}

TEST_F(FieldSetTest, AddRemove) {
  auto fields = FieldSet({});
  EXPECT_TRUE(fields.empty());
  fields.add(field_a);
  EXPECT_FALSE(fields.empty());
  fields.remove(field_a);
  EXPECT_TRUE(fields.empty());
}

TEST_F(FieldSetTest, LessOrEqual) {
  EXPECT_TRUE(FieldSet().leq(FieldSet({field_a})));
  EXPECT_TRUE(FieldSet({field_a}).leq(FieldSet({field_a, field_b})));
  EXPECT_FALSE(FieldSet({field_a}).leq(FieldSet({field_b})));

  auto fields_top = FieldSet();
  fields_top.set_to_top();
  EXPECT_TRUE(FieldSet({field_a}).leq(fields_top));
  EXPECT_FALSE(fields_top.leq(FieldSet({field_a})));
  EXPECT_TRUE(fields_top.leq(fields_top));
}

TEST_F(FieldSetTest, Equal) {
  EXPECT_TRUE(FieldSet().equals(FieldSet()));
  EXPECT_TRUE(
      FieldSet({field_a, field_b}).equals(FieldSet({field_a, field_b})));
  EXPECT_FALSE(FieldSet({field_a}).equals(FieldSet({field_a, field_c})));
  EXPECT_FALSE(
      FieldSet({field_a, field_b}).equals(FieldSet({field_a, field_c})));

  auto fields_top = FieldSet({field_a});
  fields_top.set_to_top();
  EXPECT_FALSE(fields_top.equals(FieldSet({field_a})));
}

TEST_F(FieldSetTest, Join) {
  auto fields = FieldSet();
  fields.join_with(FieldSet{field_a});
  EXPECT_EQ(fields, FieldSet({field_a}));

  fields = FieldSet({field_a});
  fields.join_with(FieldSet({field_a}));
  EXPECT_EQ(fields, FieldSet({field_a}));

  fields.join_with(FieldSet({field_b}));
  EXPECT_EQ(fields, FieldSet({field_a, field_b}));

  auto fields_top = FieldSet::top();
  fields_top.join_with(FieldSet({field_b}));
  EXPECT_TRUE(fields_top.is_top());

  fields_top.join_with(FieldSet());
  EXPECT_TRUE(fields_top.is_top());
}

TEST_F(FieldSetTest, Meet) {
  auto fields = FieldSet({field_a});
  fields.meet_with(FieldSet());
  EXPECT_EQ(fields, FieldSet());

  fields = FieldSet({field_a});
  fields.meet_with(FieldSet{field_a});
  EXPECT_EQ(fields, FieldSet({field_a}));

  fields = FieldSet({field_a});
  fields.meet_with(FieldSet{field_b});
  EXPECT_EQ(fields, FieldSet());

  fields = FieldSet({field_a});
  fields.meet_with(FieldSet{field_a, field_b});
  EXPECT_EQ(fields, FieldSet({field_a}));

  auto fields_top = FieldSet({field_a});
  fields_top.set_to_top();
  fields_top.meet_with(FieldSet({field_b}));
  EXPECT_EQ(fields_top, FieldSet({field_b}));

  fields_top = FieldSet::top();
  fields_top.meet_with(FieldSet());
  EXPECT_EQ(fields_top, FieldSet());
}

TEST_F(FieldSetTest, Difference) {
  auto fields = FieldSet();
  fields.difference_with(FieldSet{field_a});
  EXPECT_EQ(fields, FieldSet());

  fields = FieldSet({field_a});
  fields.difference_with(FieldSet{field_a});
  EXPECT_EQ(fields, FieldSet());

  fields = FieldSet({field_a});
  fields.difference_with(FieldSet{field_b});
  EXPECT_EQ(fields, FieldSet({field_a}));

  fields = FieldSet({field_a, field_b});
  fields.difference_with(FieldSet{field_a});
  EXPECT_EQ(fields, FieldSet({field_b}));

  fields = FieldSet({field_a, field_b});
  fields.difference_with(FieldSet::top());
  EXPECT_EQ(fields, FieldSet());

  auto fields_top = FieldSet::top();
  fields_top.difference_with(FieldSet{field_b});
  EXPECT_TRUE(fields_top.is_top());

  fields_top.difference_with(FieldSet());
  EXPECT_TRUE(fields_top.is_top());

  fields_top.difference_with(FieldSet::top());
  EXPECT_EQ(fields_top, FieldSet());
}

} // namespace marianatrench

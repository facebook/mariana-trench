/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <Show.h>
#include <json/json.h>

#include <mariana-trench/MethodSet.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/TaintTree.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class MethodSetTest : public test::Test {
 public:
  const Method* method_a;
  const Method* method_b;
  const Method* method_c;

  MethodSetTest() {
    Scope scope;
    auto context = test::make_empty_context();
    method_a = context.methods->create(
        redex::create_void_method(scope, "class_a", "method_a"));
    method_b = context.methods->create(
        redex::create_void_method(scope, "class_b", "method_b"));
    method_c = context.methods->create(
        redex::create_void_method(scope, "class_c", "method_c"));
  }
};

TEST_F(MethodSetTest, Constructor) {
  EXPECT_TRUE(MethodSet().is_bottom());
  EXPECT_TRUE(MethodSet().empty());
  EXPECT_FALSE(MethodSet().is_top());

  EXPECT_TRUE(MethodSet({}).is_bottom());
  EXPECT_FALSE(MethodSet({method_a}).is_bottom());
}

TEST_F(MethodSetTest, SetTopBottom) {
  auto methods = MethodSet({});
  EXPECT_FALSE(methods.is_top());
  EXPECT_TRUE(methods.is_bottom());
  methods.set_to_top();
  EXPECT_TRUE(methods.is_top());
  EXPECT_FALSE(methods.is_bottom());
  methods.set_to_bottom();
  EXPECT_FALSE(methods.is_top());
  EXPECT_TRUE(methods.is_bottom());
}

TEST_F(MethodSetTest, AddRemove) {
  auto methods = MethodSet({});
  EXPECT_TRUE(methods.empty());
  methods.add(method_a);
  EXPECT_FALSE(methods.empty());
  methods.remove(method_a);
  EXPECT_TRUE(methods.empty());
}

TEST_F(MethodSetTest, LessOrEqual) {
  EXPECT_TRUE(MethodSet().leq(MethodSet({method_a})));
  EXPECT_TRUE(MethodSet({method_a}).leq(MethodSet({method_a, method_b})));
  EXPECT_FALSE(MethodSet({method_a}).leq(MethodSet({method_b})));

  auto methods_top = MethodSet();
  methods_top.set_to_top();
  EXPECT_TRUE(MethodSet({method_a}).leq(methods_top));
  EXPECT_FALSE(methods_top.leq(MethodSet({method_a})));
  EXPECT_TRUE(methods_top.leq(methods_top));
}

TEST_F(MethodSetTest, Equal) {
  EXPECT_TRUE(MethodSet().equals(MethodSet()));
  EXPECT_TRUE(
      MethodSet({method_a, method_b}).equals(MethodSet({method_a, method_b})));
  EXPECT_FALSE(MethodSet({method_a}).equals(MethodSet({method_a, method_c})));
  EXPECT_FALSE(
      MethodSet({method_a, method_b}).equals(MethodSet({method_a, method_c})));

  auto methods_top = MethodSet({method_a});
  methods_top.set_to_top();
  EXPECT_FALSE(methods_top.equals(MethodSet({method_a})));
}

TEST_F(MethodSetTest, Join) {
  auto methods = MethodSet();
  methods.join_with(MethodSet{method_a});
  EXPECT_EQ(methods, MethodSet({method_a}));

  methods = MethodSet({method_a});
  methods.join_with(MethodSet({method_a}));
  EXPECT_EQ(methods, MethodSet({method_a}));

  methods.join_with(MethodSet({method_b}));
  EXPECT_EQ(methods, MethodSet({method_a, method_b}));

  auto methods_top = MethodSet::top();
  methods_top.join_with(MethodSet({method_b}));
  EXPECT_TRUE(methods_top.is_top());

  methods_top.join_with(MethodSet());
  EXPECT_TRUE(methods_top.is_top());
}

TEST_F(MethodSetTest, Meet) {
  auto methods = MethodSet({method_a});
  methods.meet_with(MethodSet());
  EXPECT_EQ(methods, MethodSet());

  methods = MethodSet({method_a});
  methods.meet_with(MethodSet{method_a});
  EXPECT_EQ(methods, MethodSet({method_a}));

  methods = MethodSet({method_a});
  methods.meet_with(MethodSet{method_b});
  EXPECT_EQ(methods, MethodSet());

  methods = MethodSet({method_a});
  methods.meet_with(MethodSet{method_a, method_b});
  EXPECT_EQ(methods, MethodSet({method_a}));

  auto methods_top = MethodSet({method_a});
  methods_top.set_to_top();
  methods_top.meet_with(MethodSet({method_b}));
  EXPECT_EQ(methods_top, MethodSet({method_b}));

  methods_top = MethodSet::top();
  methods_top.meet_with(MethodSet());
  EXPECT_EQ(methods_top, MethodSet());
}

TEST_F(MethodSetTest, Difference) {
  auto methods = MethodSet();
  methods.difference_with(MethodSet{method_a});
  EXPECT_EQ(methods, MethodSet());

  methods = MethodSet({method_a});
  methods.difference_with(MethodSet{method_a});
  EXPECT_EQ(methods, MethodSet());

  methods = MethodSet({method_a});
  methods.difference_with(MethodSet{method_b});
  EXPECT_EQ(methods, MethodSet({method_a}));

  methods = MethodSet({method_a, method_b});
  methods.difference_with(MethodSet{method_a});
  EXPECT_EQ(methods, MethodSet({method_b}));

  methods = MethodSet({method_a, method_b});
  methods.difference_with(MethodSet::top());
  EXPECT_EQ(methods, MethodSet());

  auto methods_top = MethodSet::top();
  methods_top.difference_with(MethodSet{method_b});
  EXPECT_TRUE(methods_top.is_top());

  methods_top.difference_with(MethodSet());
  EXPECT_TRUE(methods_top.is_top());

  methods_top.difference_with(MethodSet::top());
  EXPECT_EQ(methods_top, MethodSet());
}

} // namespace marianatrench

/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/Registry.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class HighlightTest : public test::Test {};

TEST_F(HighlightTest, TestGeneratedBounds) {
  using Bounds = Registry::Bounds;
  Scope scope;
  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "LClass;",
      /* method_name */ "method",
      /* parameter_types */ "Ljava/lang/Object;",
      /* return_type */ "Ljava/lang/Object;");
  auto* dex_method_e = redex::create_void_method(
      scope,
      /* class_name */ "LLog;",
      /* method_name */ "e",
      /* parameter_types */ "Ljava/lang/Object;",
      /* return_type */ "Ljava/lang/Object;");
  auto* dex_static_method = redex::create_void_method(
      scope,
      /* class_name */ "LClassTwo;",
      /* method_name */ "method_two",
      /* parameter_types */ "Ljava/lang/Object;",
      /* return_type */ "Ljava/lang/Object;",
      /* super */ nullptr,
      /* is_static */ true);
  DexStore store("store");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);
  auto* method_e = context.methods->get(dex_method_e);
  auto* static_method = context.methods->get(dex_static_method);
  auto registry = Registry(context, context.stores);

  auto return_port = AccessPath(Root(Root::Kind::Return));
  auto argument_port0 = AccessPath(Root(Root::Kind::Argument, 0));
  auto argument_port1 = AccessPath(Root(Root::Kind::Argument, 1));
  auto argument_port2 = AccessPath(Root(Root::Kind::Argument, 2));
  auto argument_port3 = AccessPath(Root(Root::Kind::Argument, 3));

  // Test return port
  EXPECT_EQ(
      (Bounds{2, 0, 5}),
      registry.get_highlight_bounds(method, {"", "method();"}, 2, return_port));

  // Test argument ports
  EXPECT_EQ(
      (Bounds{1, 7, 11}),
      registry.get_highlight_bounds(
          method, {"method(hello);", ""}, 1, argument_port1));
  EXPECT_EQ(
      (Bounds{3, 4, 6}),
      registry.get_highlight_bounds(
          method,
          {"method(", "    foo, ", "    bar, ", "    baz);"},
          1,
          argument_port2));
  EXPECT_EQ(
      (Bounds{2, 0, 14}),
      registry.get_highlight_bounds(
          method,
          {"method(foo, ", "new TestObject(", "arg1,", "arg2));"},
          1,
          argument_port2));
  EXPECT_EQ(
      (Bounds{3, 4, 6}),
      registry.get_highlight_bounds(
          method,
          {"method(foo(a),", "    bar(b, c),", "    baz);"},
          1,
          argument_port3));
  EXPECT_EQ(
      (Bounds{1, 11, 13}),
      registry.get_highlight_bounds(
          static_method, {"method_two(foo, bar);"}, 1, argument_port0));

  // Test 'this' (argument 0 in non-static callee)
  EXPECT_EQ(
      (Bounds{1, 0, 9}),
      registry.get_highlight_bounds(
          method, {"testObject.method();"}, 1, argument_port0));
  EXPECT_EQ(
      (Bounds{1, 0, 5}),
      registry.get_highlight_bounds(method, {"method();"}, 1, argument_port0));
  EXPECT_EQ(
      (Bounds{3, 5, 10}),
      registry.get_highlight_bounds(
          method,
          {"result = testObject.transform1(arg1)",
           "    .transform2(arg2)",
           "    .method(arg);"},
          3,
          argument_port0));

  // Test that we do not highlight first occurrence of callee's name in line,
  // but the first call of it. Here we should not highlight the 'e' in
  // testObject (solved by searching for callee_name + '(')
  EXPECT_EQ(
      (Bounds{1, 11, 11}),
      registry.get_highlight_bounds(
          method_e, {"testObject.e();"}, 1, return_port));

  // Wrong line provided
  EXPECT_EQ(
      (Bounds{1, 0, 0}),
      registry.get_highlight_bounds(method, {"", ""}, 1, return_port));
  EXPECT_EQ(
      (Bounds{2, 0, 0}),
      registry.get_highlight_bounds(
          method, {"method(foo, ", "bar);"}, 2, return_port));
  EXPECT_EQ(
      (Bounds{0, 0, 0}),
      registry.get_highlight_bounds(method, {"method()"}, 0, return_port));
  EXPECT_EQ(
      (Bounds{3, 0, 0}),
      registry.get_highlight_bounds(method, {"method()"}, 3, return_port));

  // Invalid java provided
  EXPECT_EQ(
      (Bounds{1, 0, 5}),
      registry.get_highlight_bounds(method, {"method("}, 1, argument_port1));
  EXPECT_EQ(
      (Bounds{1, 0, 5}),
      registry.get_highlight_bounds(
          method, {"method(", "foo,"}, 1, argument_port2));
  EXPECT_EQ(
      (Bounds{1, 0, 5}),
      registry.get_highlight_bounds(
          method, {"method(", "foo);"}, 1, argument_port2));
}

} // namespace marianatrench

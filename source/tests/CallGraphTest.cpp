/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>
#include <algorithm>

#include <gtest/gtest.h>
#include <mariana-trench/CallGraph.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

namespace {

bool call_target_indices_are_equal(
    const std::vector<CallTarget>& actual,
    const std::vector<CallTarget>& expected) {
  if (actual.size() != expected.size()) {
    return false;
  }

  return std::is_permutation(
      actual.begin(),
      actual.end(),
      expected.begin(),
      expected.end(),
      [](const CallTarget& target1, const CallTarget& target2) {
        return target1.resolved_base_callee() ==
            target2.resolved_base_callee() &&
            target1.call_index() == target2.call_index();
      });
}

} // namespace

class CallGraphTest : public test::Test {};

TEST_F(CallGraphTest, CallIndices) {
  Scope scope;

  auto* dex_callee = redex::create_void_method(scope, "LUtil;", "call");
  auto* inherited_dex_method = redex::create_void_method(
      scope,
      "LParent;",
      "inherited_method",
      /* paramater_types */ "",
      /* return_type */ "V",
      /* super */ nullptr,
      /* is_static */ true);
  redex::create_methods(
      scope,
      "LChild1;",
      std::vector<std::string>{},
      inherited_dex_method->get_class());
  redex::create_methods(
      scope,
      "LChild2;",
      std::vector<std::string>{},
      inherited_dex_method->get_class());
  auto* dex_method = redex::create_method(scope, "LMainActivity;", R"(
    (method (public) "LMainActivity;.onCreate:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LUtil;.call:()V")
      (invoke-direct (v0) "LUtil;.call:()V")
      (invoke-static () "LParent;.inherited_method:()V")
      (invoke-static () "LChild1;.inherited_method:()V")
      (invoke-static () "LChild2;.inherited_method:()V")
      (return-void)
     )
    )
  )");
  DexStore store("stores");
  store.add_classes(scope);

  auto context = test::make_context(store);
  const auto* callee = context.methods->get(dex_callee);
  const auto* method = context.methods->get(dex_method);
  const auto* inherited_method = context.methods->get(inherited_dex_method);
  auto callees = context.call_graph->callees(method);
  EXPECT_TRUE(call_target_indices_are_equal(
      callees,
      {
          CallTarget::virtual_call(
              /* intruction */ nullptr,
              callee,
              /* call_index */ 0,
              /* receiver_type */ nullptr,
              *context.class_hierarchies,
              *context.overrides),
          CallTarget::virtual_call(
              /* intruction */ nullptr,
              callee,
              /* call_index */ 1,
              /* receiver_type */ nullptr,
              *context.class_hierarchies,
              *context.overrides),
          // Call targets count the raw callee from the instruction rather than
          // the resolved callee
          CallTarget::virtual_call(
              /* intruction */ nullptr,
              inherited_method,
              /* call_index */ 0,
              /* receiver_type */ nullptr,
              *context.class_hierarchies,
              *context.overrides),
          CallTarget::virtual_call(
              /* intruction */ nullptr,
              inherited_method,
              /* call_index */ 0,
              /* receiver_type */ nullptr,
              *context.class_hierarchies,
              *context.overrides),
          CallTarget::virtual_call(
              /* intruction */ nullptr,
              inherited_method,
              /* call_index */ 0,
              /* receiver_type */ nullptr,
              *context.class_hierarchies,
              *context.overrides),
      }));
}

} // namespace marianatrench

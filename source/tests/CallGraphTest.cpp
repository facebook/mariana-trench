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
  redex::create_class(
      scope,
      "LChild1;",
      /* super */ inherited_dex_method->get_class());
  redex::create_class(
      scope,
      "LChild2;",
      /* super */ inherited_dex_method->get_class());
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

TEST_F(CallGraphTest, ArtificialCallIndices) {
  Scope scope;

  redex::create_void_method(scope, "LUtil;", "call");
  auto anonymous_class_callees = redex::create_methods(
      scope,
      "LMainActivity$1;",
      {R"(
        (method (public) "LMainActivity$1;.method1:()V"
        (
          (load-param-object v0)
          (invoke-direct (v0) "Ljava/lang/Object;.<init>:()V")
          (return-void)
        ))
      )",
       R"(
        (method (public) "LMainActivity$1;.method2:()V"
        (
          (load-param-object v0)
          (invoke-direct (v0) "LUtil;.call:()V")
          (return-void)
        ))
    )"});
  auto anonymous_class_for_iput_callees = redex::create_methods(
      scope,
      "LMainActivity$2;",
      {R"(
        (method (public) "LMainActivity$2;.method3:()V"
        (
          (load-param-object v0)
          (invoke-direct (v0) "Ljava/lang/Object;.<init>:()V")
          (return-void)
        ))
      )",
       R"(
        (method (public) "LMainActivity$2;.method4:()V"
        (
          (load-param-object v0)
          (invoke-direct (v0) "LUtil;.call:()V")
          (return-void)
        ))
    )"});

  // When a method with no code (external method/abstract method) gets an
  // anonymous class as a callee, then we add artificial calls to all of its
  // methods
  redex::create_void_method(
      scope,
      "LThing;",
      "method",
      /* parameter_types */ "LRunnable;",
      /* return_type */ "V",
      /* super */ nullptr,
      /* is_static */ true,
      /* is_private */ false,
      /* is_native */ false,
      /* is_abstract */ true);

  auto* dex_method = redex::create_method(scope, "LMainActivity;", R"(
    (method (public) "LMainActivity;.onCreate:()V"
     (
      (load-param-object v0)
      (new-instance "LMainActivity$1;")
      (move-result-pseudo-object v1)
      (invoke-static (v1) "LThing;.method:(LRunnable;)V")
      (invoke-static (v1) "LThing;.method:(LRunnable;)V")
      (new-instance "LMainActivity$2;")
      (move-result-pseudo-object v2)
      (iput-object v2 v0 "LMainActivity;.field:Ljava/jang/Object;")
      (return-void)
     )
    )
  )");
  DexStore store("stores");
  store.add_classes(scope);

  auto context = test::make_context(store);
  const auto* method = context.methods->get(dex_method);
  const auto* anonymous_callee1 =
      context.methods->get(anonymous_class_callees[0]);
  const auto* anonymous_callee2 =
      context.methods->get(anonymous_class_callees[1]);

  std::vector<CallTarget> artificial_callee_targets;
  for (const auto& [_, artificial_callees] :
       context.call_graph->artificial_callees(method)) {
    for (const auto& callee : artificial_callees) {
      artificial_callee_targets.push_back(callee.call_target);
    }
  }
  EXPECT_TRUE(call_target_indices_are_equal(
      artificial_callee_targets,
      {
          // Providing an anonymous class as an arg to a method with no code
          // causes artificial callees
          CallTarget::static_call(
              /* intruction */ nullptr,
              anonymous_callee1,
              /* call_index */ 0),
          CallTarget::static_call(
              /* intruction */ nullptr,
              anonymous_callee1,
              /* call_index */ 1),
          CallTarget::static_call(
              /* intruction */ nullptr,
              anonymous_callee2,
              /* call_index */ 0),
          CallTarget::static_call(
              /* intruction */ nullptr,
              anonymous_callee2,
              /* call_index */ 1),
          // Assigning an anonymous class to a field causes artificial callees
          CallTarget::static_call(
              /* intruction */ nullptr,
              context.methods->get(anonymous_class_for_iput_callees[0]),
              /* call_index */ 0),
          CallTarget::static_call(
              /* intruction */ nullptr,
              context.methods->get(anonymous_class_for_iput_callees[1]),
              /* call_index */ 0),
      }));
}

TEST_F(CallGraphTest, ShimCallIndices) {
  Scope scope;

  auto shimmed_method1 =
      redex::create_void_method(scope, "LShimmed1;", "method");
  auto shimmed_method2 = redex::create_void_method(
      scope,
      "LShimmed2;",
      "static_method",
      /* parameter_types */ "",
      /* return_type */ "V",
      /* super */ nullptr,
      /* is_static */ true);

  // Note: shim is defined in file: tests/shims.json
  redex::create_void_method(
      scope, "LExample;", "methodToShim", /* parameter_types */ "LShimmed1;");

  auto* dex_method = redex::create_method(scope, "LMainActivity;", R"(
    (method (public) "LMainActivity;.onCreate:()V"
     (
      (load-param-object v0)
      (new-instance "LShimmed1;")
      (move-result-pseudo-object v1)
      (invoke-direct (v0 v1) "LExample;.methodToShim:(LShimmed1;)V")
      (invoke-direct (v0 v1) "LExample;.methodToShim:(LShimmed1;)V")
      (return-void)
     )
    )
  )");
  DexStore store("stores");
  store.add_classes(scope);

  auto context = test::make_context(store);
  const auto* method = context.methods->get(dex_method);
  std::vector<CallTarget> artificial_callee_targets;
  for (const auto& [_, artificial_callees] :
       context.call_graph->artificial_callees(method)) {
    for (const auto& callee : artificial_callees) {
      artificial_callee_targets.push_back(callee.call_target);
    }
  }
  EXPECT_TRUE(call_target_indices_are_equal(
      artificial_callee_targets,
      {
          CallTarget::virtual_call(
              /* intruction */ nullptr,
              context.methods->get(shimmed_method1),
              /* call_index */ 0,
              /* receiver_type */ nullptr,
              *context.class_hierarchies,
              *context.overrides),
          CallTarget::static_call(
              /* intruction */ nullptr,
              context.methods->get(shimmed_method2),
              /* call_index */ 0),
          CallTarget::virtual_call(
              /* intruction */ nullptr,
              context.methods->get(shimmed_method1),
              /* call_index */ 1,
              /* receiver_type */ nullptr,
              *context.class_hierarchies,
              *context.overrides),
          CallTarget::static_call(
              /* intruction */ nullptr,
              context.methods->get(shimmed_method2),
              /* call_index */ 1),
      }));
}

TEST_F(CallGraphTest, FieldIndices) {
  Scope scope;
  auto* dex_inherited_field = redex::create_field(
      scope, "LParent;", {"mInherited", type::java_lang_Object()});
  redex::create_class(
      scope, "LChild1;", /* super */ dex_inherited_field->get_class());
  redex::create_class(
      scope, "LChild2;", /* super */ dex_inherited_field->get_class());
  auto* dex_static_field = redex::create_field(
      scope,
      "LClass;",
      {"mStatic", type::java_lang_Object()},
      /* super */ nullptr,
      /* is_static */ true);

  auto* dex_method = redex::create_method(scope, "LMainActivity;", R"(
    (method (public) "LMainActivity;.onCreate:()V"
     (
      (load-param-object v0)
      (new-instance "Ljava/lang/Object;")
      (move-result-pseudo-object v1)
      (sput-object v1 "LClass;.mStatic:Ljava/lang/Object;")
      (new-instance "LParent;")
      (move-result-pseudo-object v2)
      (iput-object v1 v2 "LParent;.mInherited:Ljava/lang/Object;")
      (new-instance "LChild1;")
      (move-result-pseudo-object v2)
      (iput-object v1 v2 "LChild1;.mInherited:Ljava/lang/Object;")
      (new-instance "LChild2;")
      (move-result-pseudo-object v2)
      (iput-object v1 v2 "LChild2;.mInherited:Ljava/lang/Object;")
      (iput-object v1 v2 "LChild2;.mInherited:Ljava/lang/Object;")
      (sput-object v1 "LClass;.mStatic:Ljava/lang/Object;")
      (return-void)
     )
    )
  )");
  DexStore store("stores");
  store.add_classes(scope);

  auto context = test::make_context(store);
  const auto* method = context.methods->get(dex_method);
  const auto* inherited_field = context.fields->get(dex_inherited_field);
  const auto* static_field = context.fields->get(dex_static_field);
  auto field_targets = context.call_graph->resolved_field_accesses(method);
  std::vector<FieldTarget> expected_targets = {
      FieldTarget{static_field, 0},
      FieldTarget{inherited_field, 0},
      FieldTarget{inherited_field, 0},
      FieldTarget{inherited_field, 0},
      FieldTarget{inherited_field, 1},
      FieldTarget{static_field, 1}};
  EXPECT_TRUE(std::is_permutation(
      field_targets.begin(),
      field_targets.end(),
      expected_targets.begin(),
      expected_targets.end()));
}

TEST_F(CallGraphTest, ReturnIndices) {
  Scope scope;
  redex::create_class(
      scope, "LSomething;", /* super */ type::java_lang_Object());
  auto* dex_method = redex::create_method(scope, "LMainActivity;", R"(
    (method (public) "LMainActivity;.someMethod:(I)Ljava/lang/Object;"
     (
      (load-param v4)
      (load-param v0)
      (const-wide v1 0)
      (cmp-long v2 v0 v1)
      (if-lez v2 :true)
      (new-instance "Ljava/lang/Object;")
      (move-result-pseudo-object v3)
      (return-object v3)
      (:true)
      (new-instance "LSomething;")
      (move-result-pseudo-object v3)
      (return-object v3)
     )
    )
  )");

  DexStore store("stores");
  store.add_classes(scope);

  auto context = test::make_context(store);
  const auto* method = context.methods->get(dex_method);
  auto return_indices = context.call_graph->return_indices(method);
  std::vector<TextualOrderIndex> expected_return_indices = {0, 1};
  EXPECT_TRUE(std::is_permutation(
      return_indices.begin(),
      return_indices.end(),
      expected_return_indices.begin(),
      expected_return_indices.end()));
}

TEST_F(CallGraphTest, ArrayAllocation) {
  Scope scope;
  redex::create_class(
      scope, "LSomething;", /* super */ type::java_lang_Object());
  auto* dex_method = redex::create_method(scope, "LMainActivity;", R"(
    (method (public) "LMainActivity;.someMethod:()V"
     (
      (load-param v0)
      (const v1 10)
      (new-array v1 "[I")
      (move-result-pseudo-object v2)

      (const-string "hello")
      (move-result-pseudo-object v3)
      (check-cast v3 "Ljava/lang/String;")
      (move-result-pseudo-object v4)
      (filled-new-array (v5) "[Ljava/lang/String;")
      (move-result-object v6)

      (new-array v1 "[I")
      (move-result-pseudo-object v7)

      (const v8 2)
      (filled-new-array (v8) "[Ljava/lang/String;")
      (move-result-object v9)

      (return-void)
     )
    )
  )");

  DexStore store("stores");
  store.add_classes(scope);

  auto context = test::make_context(store);
  const auto* method = context.methods->get(dex_method);
  auto array_allocation_indices =
      context.call_graph->array_allocation_indices(method);
  std::vector<TextualOrderIndex> expected_array_allocation_indices = {
      0, 1, 2, 3};
  EXPECT_TRUE(std::is_permutation(
      array_allocation_indices.begin(),
      array_allocation_indices.end(),
      expected_array_allocation_indices.begin(),
      expected_array_allocation_indices.end()));
}

} // namespace marianatrench

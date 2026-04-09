/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <DexStore.h>
#include <Show.h>

#include <mariana-trench/ControlFlowGraphs.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/local_flow/LocalFlowMethodAnalyzer.h>
#include <mariana-trench/local_flow/LocalFlowTransfer.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;
using namespace marianatrench::local_flow;

namespace {

class LocalFlowTransferTest : public test::Test {};

// Helper: analyze a method and return its constraints
std::optional<LocalFlowMethodResult> analyze_method(
    const Scope& scope,
    const DexMethod* dex_method) {
  DexStore store("test_store");
  store.add_classes(scope);
  // The ControlFlowGraphs constructor has side effects on DexMethods.
  ControlFlowGraphs({store});
  auto methods = Methods({store});
  auto* method = methods.get(dex_method);
  return LocalFlowMethodAnalyzer::analyze(method, /* max_structural_depth */ 5);
}

// Helper: check if any constraint has a source or target with the given string
bool has_node_string(
    const LocalFlowConstraintSet& constraints,
    const std::string& needle) {
  for (const auto& c : constraints.constraints()) {
    if (c.source.to_string().find(needle) != std::string::npos ||
        c.target.to_string().find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

// Helper: check if any constraint has a Call+ label
bool has_call_plus(const LocalFlowConstraintSet& constraints) {
  for (const auto& c : constraints.constraints()) {
    for (const auto& l : c.labels) {
      if (l.kind == LabelKind::Call && l.preserves_context) {
        return true;
      }
    }
  }
  return false;
}

// Helper: check if any constraint has a Call (not +) label
bool has_call_no_context(const LocalFlowConstraintSet& constraints) {
  for (const auto& c : constraints.constraints()) {
    for (const auto& l : c.labels) {
      if (l.kind == LabelKind::Call && !l.preserves_context) {
        return true;
      }
    }
  }
  return false;
}

// Helper: check if any constraint references a Global node with given name
bool has_global_node(
    const LocalFlowConstraintSet& constraints,
    const std::string& needle) {
  for (const auto& c : constraints.constraints()) {
    if ((c.source.kind() == NodeKind::Global &&
         c.source.to_string().find(needle) != std::string::npos) ||
        (c.target.kind() == NodeKind::Global &&
         c.target.to_string().find(needle) != std::string::npos)) {
      return true;
    }
  }
  return false;
}

// Helper: check if any constraint references a Meth node with given name
bool has_meth_node(
    const LocalFlowConstraintSet& constraints,
    const std::string& needle) {
  for (const auto& c : constraints.constraints()) {
    if ((c.source.kind() == NodeKind::Meth &&
         c.source.to_string().find(needle) != std::string::npos) ||
        (c.target.kind() == NodeKind::Meth &&
         c.target.to_string().find(needle) != std::string::npos)) {
      return true;
    }
  }
  return false;
}

// Helper: check if any constraint has a ThisOut target node
bool has_this_out_target(const LocalFlowConstraintSet& constraints) {
  for (const auto& c : constraints.constraints()) {
    if (c.target.kind() == NodeKind::ThisOut) {
      return true;
    }
  }
  return false;
}

// Helper: check if any constraint has a SelfRec node
bool has_self_rec_node(const LocalFlowConstraintSet& constraints) {
  for (const auto& c : constraints.constraints()) {
    if (c.source.kind() == NodeKind::SelfRec ||
        c.target.kind() == NodeKind::SelfRec) {
      return true;
    }
  }
  return false;
}

// Helper: check if any constraint has a Project:this_out label
bool has_project_this_out(const LocalFlowConstraintSet& constraints) {
  for (const auto& c : constraints.constraints()) {
    for (const auto& l : c.labels) {
      if (l.kind == LabelKind::Project && l.value == "this_out") {
        return true;
      }
    }
  }
  return false;
}

} // namespace

TEST_F(LocalFlowTransferTest, StaticCallUsesGlobalNode) {
  Scope scope;

  // Use a static method with one param so the Call constraint survives
  // temp elimination (Param -> Global is non-temp to non-temp)
  marianatrench::redex::create_void_method(
      scope,
      "LCallee;",
      "staticMethod",
      "Ljava/lang/String;",
      "V",
      nullptr,
      /* is_static */ true);

  auto* dex_method = marianatrench::redex::create_method(
      scope,
      "LCaller;",
      R"#(
        (method (public static) "LCaller;.test:(Ljava/lang/String;)V"
         (
          (load-param-object v0)
          (invoke-static (v0) "LCallee;.staticMethod:(Ljava/lang/String;)V")
          (return-void)
         )
        )
      )#");

  auto result = analyze_method(scope, dex_method);
  ASSERT_TRUE(result.has_value());
  const auto& constraints = result->constraints;

  // Static call uses Global node (not Meth)
  EXPECT_TRUE(has_global_node(constraints, "staticMethod"));
  EXPECT_FALSE(has_meth_node(constraints, "staticMethod"));
  // Static call never preserves context
  EXPECT_TRUE(has_call_no_context(constraints));
  EXPECT_FALSE(has_call_plus(constraints));
}

TEST_F(LocalFlowTransferTest, VirtualCallOnThisUsesCallPlus) {
  Scope scope;

  // Virtual call on this: receiver arg generates Call+ constraint
  // Param(0) -[Inject:this, Call+]-> Meth(callee) survives temp elimination
  auto dex_methods = marianatrench::redex::create_methods(
      scope,
      "LBase;",
      {R"#(
        (method (public) "LBase;.caller:()V"
         (
          (load-param-object v0)
          (invoke-virtual (v0) "LBase;.virtualMethod:()V")
          (return-void)
         )
        )
      )#",
       R"#(
        (method (public) "LBase;.virtualMethod:()V"
         (
          (load-param-object v0)
          (return-void)
         )
        )
      )#"});

  auto* dex_method = dex_methods[0];
  auto result = analyze_method(scope, dex_method);
  ASSERT_TRUE(result.has_value());
  const auto& constraints = result->constraints;

  EXPECT_TRUE(has_meth_node(constraints, "virtualMethod"));
  EXPECT_TRUE(has_call_plus(constraints));
  // Return+ doesn't survive temp elimination for void methods
  // (callee -> result_temp is a dead-end temp)
}

TEST_F(LocalFlowTransferTest, VirtualCallOnNonThisUsesCallNoContext) {
  Scope scope;

  marianatrench::redex::create_void_method(scope, "LOther;", "virtualMethod");

  auto* dex_method = marianatrench::redex::create_method(
      scope,
      "LCaller;",
      R"#(
        (method (public) "LCaller;.test:(LOther;)V"
         (
          (load-param-object v0)
          (load-param-object v1)
          (invoke-virtual (v1) "LOther;.virtualMethod:()V")
          (return-void)
         )
        )
      )#");

  auto result = analyze_method(scope, dex_method);
  ASSERT_TRUE(result.has_value());
  const auto& constraints = result->constraints;

  EXPECT_TRUE(has_meth_node(constraints, "virtualMethod"));
  EXPECT_TRUE(has_call_no_context(constraints));
  EXPECT_FALSE(has_call_plus(constraints));
}

TEST_F(LocalFlowTransferTest, SuperCallUsesGlobalNodeAndCallPlus) {
  Scope scope;

  auto* dex_base =
      marianatrench::redex::create_void_method(scope, "LSuperBase;", "method");

  auto* dex_method = marianatrench::redex::create_method(
      scope,
      "LSuperChild;",
      R"#(
        (method (public) "LSuperChild;.method:()V"
         (
          (load-param-object v0)
          (invoke-super (v0) "LSuperBase;.method:()V")
          (return-void)
         )
        )
      )#",
      dex_base->get_class());

  auto result = analyze_method(scope, dex_method);
  ASSERT_TRUE(result.has_value());
  const auto& constraints = result->constraints;

  // Super call is monomorphic -> Global node
  EXPECT_TRUE(has_global_node(constraints, "method"));
  EXPECT_FALSE(has_meth_node(constraints, "method"));
  // Super call preserves context (receiver is this)
  EXPECT_TRUE(has_call_plus(constraints));
}

TEST_F(LocalFlowTransferTest, ConstructorCallUsesGlobalNoContext) {
  Scope scope;

  marianatrench::redex::create_void_method(scope, "LFoo;", "<init>");

  auto* dex_method = marianatrench::redex::create_method(
      scope,
      "LCtorCaller;",
      R"#(
        (method (public static) "LCtorCaller;.test:()LFoo;"
         (
          (new-instance "LFoo;")
          (move-result-pseudo-object v0)
          (invoke-direct (v0) "LFoo;.<init>:()V")
          (return-object v0)
         )
        )
      )#");

  auto result = analyze_method(scope, dex_method);
  ASSERT_TRUE(result.has_value());
  const auto& constraints = result->constraints;

  // <init> does NOT preserve context
  EXPECT_FALSE(has_call_plus(constraints));
}

TEST_F(LocalFlowTransferTest, PrivateCallOnThisUsesCallPlusAndMeth) {
  Scope scope;

  auto dex_methods = marianatrench::redex::create_methods(
      scope,
      "LMyClass;",
      {R"#(
        (method (public) "LMyClass;.caller:()V"
         (
          (load-param-object v0)
          (invoke-direct (v0) "LMyClass;.privateHelper:()V")
          (return-void)
         )
        )
      )#",
       R"#(
        (method (private) "LMyClass;.privateHelper:()V"
         (
          (load-param-object v0)
          (return-void)
         )
        )
      )#"});

  auto* dex_method = dex_methods[0];
  auto result = analyze_method(scope, dex_method);
  ASSERT_TRUE(result.has_value());
  const auto& constraints = result->constraints;

  // Private method on this uses Meth node (not Global)
  EXPECT_TRUE(has_meth_node(constraints, "privateHelper"));
  EXPECT_FALSE(has_global_node(constraints, "privateHelper"));
  // Private method on this preserves context
  EXPECT_TRUE(has_call_plus(constraints));
}

TEST_F(LocalFlowTransferTest, BranchJoinMergesBothPaths) {
  Scope scope;

  auto* dex_method = marianatrench::redex::create_method(
      scope,
      "LBranch;",
      R"#(
        (method (public static) "LBranch;.test:(ZLjava/lang/String;Ljava/lang/String;)Ljava/lang/String;"
         (
          (load-param v0)
          (load-param-object v1)
          (load-param-object v2)
          (if-eqz v0 :else)
          (move-object v3 v1)
          (goto :end)
          (:else)
          (move-object v3 v2)
          (:end)
          (return-object v3)
         )
        )
      )#");

  auto result = analyze_method(scope, dex_method);
  ASSERT_TRUE(result.has_value());
  const auto& constraints = result->constraints;

  // Both p1 (v1) and p2 (v2) should flow to result
  // (p0 is the boolean flag, p1 and p2 are the String params)
  // After temp elimination, we should see p1 and p2 connected
  // to result (through join temps)
  bool has_param1 = has_node_string(constraints, "p1");
  bool has_param2 = has_node_string(constraints, "p2");
  bool has_result_node = has_node_string(constraints, "result");

  EXPECT_TRUE(has_param1);
  EXPECT_TRUE(has_param2);
  EXPECT_TRUE(has_result_node);
}

TEST_F(LocalFlowTransferTest, VirtualCallEmitsThisOut) {
  Scope scope;

  // Instance method calling a virtual method on a non-this receiver.
  // The callee's this_out should flow back to the receiver register.
  auto dex_methods = marianatrench::redex::create_methods(
      scope,
      "LSetter;",
      {R"#(
        (method (public) "LSetter;.setValue:(Ljava/lang/Object;)V"
         (
          (load-param-object v0)
          (load-param-object v1)
          (iput-object v1 v0 "LSetter;.value:Ljava/lang/Object;")
          (return-void)
         )
        )
      )#",
       R"#(
        (method (public static) "LSetter;.test:(LSetter;Ljava/lang/Object;)LSetter;"
         (
          (load-param-object v0)
          (load-param-object v1)
          (invoke-virtual (v0 v1) "LSetter;.setValue:(Ljava/lang/Object;)V")
          (return-object v0)
         )
        )
      )#"});

  auto* dex_method = dex_methods[1]; // test method
  auto result = analyze_method(scope, dex_method);
  ASSERT_TRUE(result.has_value());
  const auto& constraints = result->constraints;

  // Non-init invoke on any receiver should now emit this_out flow-back
  EXPECT_TRUE(has_project_this_out(constraints));
}

TEST_F(LocalFlowTransferTest, VoidSetterEmitsThisOut) {
  Scope scope;

  // An instance setter method itself should emit this -> this_out at exit
  auto* dex_method = marianatrench::redex::create_method(
      scope,
      "LBuilder;",
      R"#(
        (method (public) "LBuilder;.setName:(Ljava/lang/String;)V"
         (
          (load-param-object v0)
          (load-param-object v1)
          (iput-object v1 v0 "LBuilder;.name:Ljava/lang/String;")
          (return-void)
         )
        )
      )#");

  auto result = analyze_method(scope, dex_method);
  ASSERT_TRUE(result.has_value());
  const auto& constraints = result->constraints;

  // Instance method should have this_out target from the exit state
  EXPECT_TRUE(has_this_out_target(constraints));
}

TEST_F(LocalFlowTransferTest, SelfRecursiveCallUsesSelfRecNode) {
  Scope scope;

  // A method that calls itself recursively should use SelfRec node
  auto* dex_method = marianatrench::redex::create_method(
      scope,
      "LRec;",
      R"#(
        (method (public static) "LRec;.process:(Ljava/lang/Object;)Ljava/lang/Object;"
         (
          (load-param-object v0)
          (if-eqz v0 :base)
          (invoke-static (v0) "LRec;.process:(Ljava/lang/Object;)Ljava/lang/Object;")
          (move-result-object v1)
          (return-object v1)
          (:base)
          (const v2 0)
          (return-object v2)
         )
        )
      )#");

  auto result = analyze_method(scope, dex_method);
  ASSERT_TRUE(result.has_value());
  const auto& constraints = result->constraints;

  // Self-recursive call uses SelfRec node
  EXPECT_TRUE(has_self_rec_node(constraints));
  // Should NOT have Global or Meth for the same method
  EXPECT_FALSE(has_global_node(constraints, "process"));
  EXPECT_FALSE(has_meth_node(constraints, "process"));
}

TEST_F(LocalFlowTransferTest, MutualRecursionUsesNormalNode) {
  Scope scope;

  marianatrench::redex::create_void_method(
      scope,
      "LMutual;",
      "other",
      "Ljava/lang/Object;",
      "Ljava/lang/Object;",
      nullptr,
      /* is_static */ true);

  auto* dex_method = marianatrench::redex::create_method(
      scope,
      "LMutualB;",
      R"#(
        (method (public static) "LMutualB;.process:(Ljava/lang/Object;)Ljava/lang/Object;"
         (
          (load-param-object v0)
          (invoke-static (v0) "LMutual;.other:(Ljava/lang/Object;)Ljava/lang/Object;")
          (move-result-object v1)
          (return-object v1)
         )
        )
      )#");

  auto result = analyze_method(scope, dex_method);
  ASSERT_TRUE(result.has_value());
  const auto& constraints = result->constraints;

  // Mutual recursion should NOT use SelfRec
  EXPECT_FALSE(has_self_rec_node(constraints));
  // Should use Global node for static call to a different method
  EXPECT_TRUE(has_global_node(constraints, "other"));
}

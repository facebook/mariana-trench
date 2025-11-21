/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <optional>

#include <gtest/gtest.h>

#include <Creators.h>
#include <DexStore.h>
#include <IRAssembler.h>
#include <RedexContext.h>

#include <mariana-trench/Dependencies.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {

class DependenciesTest : public test::Test {};

Context test_dependencies(const Scope& scope) {
  Context context;
  context.options = std::make_unique<Options>(
      /* models_path */ std::vector<std::string>{},
      /* field_models_path */ std::vector<std::string>{},
      /* literal_models_path */ std::vector<std::string>{},
      /* rules_path */ std::vector<std::string>{},
      /* lifecycles_path */ std::vector<std::string>{},
      /* shims_path */ std::vector<std::string>{},
      /* graphql_metadata_paths */ std::string{},
      /* proguard_configuration_paths */ std::vector<std::string>{},
      /* sequential */ false,
      /* skip_source_indexing */ true,
      /* skip_analysis */ true,
      /* model_generators_configuration */
      std::vector<ModelGeneratorConfiguration>{},
      /* model_generator_search_paths */ std::vector<std::string>{},
      /* remove_unreachable_code */ false,
      /* emit_all_via_cast_features */ false);
  DexStore store("test_store");
  store.add_classes(scope);
  context.stores = {store};
  context.artificial_methods = std::make_unique<ArtificialMethods>(
      *context.kind_factory, context.stores);
  context.methods = std::make_unique<Methods>(context.stores);
  MethodMappings method_mappings{*context.methods};
  context.control_flow_graphs =
      std::make_unique<ControlFlowGraphs>(context.stores);
  context.types = std::make_unique<Types>(*context.options, context.stores);
  context.class_hierarchies = std::make_unique<ClassHierarchies>(
      *context.options, context.options->analysis_mode(), context.stores);
  context.overrides = std::make_unique<Overrides>(
      *context.options,
      context.options->analysis_mode(),
      *context.methods,
      context.stores);
  context.fields = std::make_unique<Fields>();
  context.call_graph = std::make_unique<CallGraph>(
      *context.options,
      *context.types,
      *context.class_hierarchies,
      *context.feature_factory,
      *context.heuristics,
      *context.methods,
      *context.fields,
      *context.overrides,
      method_mappings,
      LifecycleMethods{},
      Shims{/* global_shims_size */ 0});
  context.rules = std::make_unique<Rules>(context);
  auto registry = Registry(context);
  context.dependencies = std::make_unique<Dependencies>(
      *context.options,
      *context.heuristics,
      *context.methods,
      *context.overrides,
      *context.call_graph,
      registry);
  return context;
}

std::unordered_set<const Method*> resolved_base_callees(
    const std::vector<CallTarget>& call_targets) {
  std::unordered_set<const Method*> callees;
  for (const auto& call_target : call_targets) {
    callees.insert(call_target.resolved_base_callee());
  }
  return callees;
}

} // anonymous namespace

TEST_F(DependenciesTest, InvokeVirtual) {
  Scope scope;

  auto* dex_callee =
      marianatrench::redex::create_void_method(scope, "LCallee;", "callee");
  auto* dex_caller = marianatrench::redex::create_method(scope, "LCaller;", R"(
    (method (public) "LCaller;.caller:()V"
     (
      (load-param-object v0)
      (invoke-virtual (v0) "LCallee;.callee:()V")
      (return-void)
     )
    )
  )");

  auto context = test_dependencies(scope);
  const auto& call_graph = *context.call_graph;
  const auto& dependencies = *context.dependencies;
  auto* callee = context.methods->get(dex_callee);
  auto* caller = context.methods->get(dex_caller);

  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(caller)),
      testing::UnorderedElementsAre(callee));
  EXPECT_TRUE(call_graph.callees(callee).empty());

  EXPECT_TRUE(dependencies.dependencies(caller).empty());
  EXPECT_THAT(
      dependencies.dependencies(callee), testing::UnorderedElementsAre(caller));
}

TEST_F(DependenciesTest, InvokeDirect) {
  Scope scope;

  auto* dex_callee =
      marianatrench::redex::create_void_method(scope, "LCallee;", "callee");
  auto* dex_caller = marianatrench::redex::create_method(scope, "LCaller;", R"(
    (method (public) "LCaller;.caller:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LCallee;.callee:()V")
      (return-void)
     )
    )
  )");

  auto context = test_dependencies(scope);
  const auto& call_graph = *context.call_graph;
  const auto& dependencies = *context.dependencies;
  auto* callee = context.methods->get(dex_callee);
  auto* caller = context.methods->get(dex_caller);

  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(caller)),
      testing::UnorderedElementsAre(callee));
  EXPECT_TRUE(call_graph.callees(callee).empty());

  EXPECT_TRUE(call_graph.artificial_callees(caller).empty());
  EXPECT_TRUE(call_graph.artificial_callees(callee).empty());

  EXPECT_TRUE(dependencies.dependencies(caller).empty());
  EXPECT_THAT(
      dependencies.dependencies(callee), testing::UnorderedElementsAre(caller));
}

TEST_F(DependenciesTest, Recursion) {
  Scope scope;

  auto* dex_recursive = marianatrench::redex::create_method(
      scope,
      "LRecursive;",
      R"(
        (method (public) "LRecursive;.recursive:()V"
         (
          (load-param-object v0)
          (invoke-direct (v0) "LRecursive;.recursive:()V")
          (return-void)
         )
        )
      )");

  auto context = test_dependencies(scope);
  const auto& call_graph = *context.call_graph;
  const auto& dependencies = *context.dependencies;
  auto* recursive = context.methods->get(dex_recursive);

  EXPECT_EQ(call_graph.callees(recursive).size(), 1);
  EXPECT_EQ(
      resolved_base_callees(call_graph.callees(recursive)).count(recursive), 1);

  EXPECT_TRUE(call_graph.artificial_callees(recursive).empty());

  EXPECT_EQ(dependencies.dependencies(recursive).size(), 1);
  EXPECT_EQ(dependencies.dependencies(recursive).count(recursive), 1);
}

TEST_F(DependenciesTest, MultipleCallees) {
  Scope scope;

  auto* dex_callee_one =
      marianatrench::redex::create_void_method(scope, "LCalleeOne;", "callee");
  auto* dex_callee_two =
      marianatrench::redex::create_void_method(scope, "LCalleeTwo;", "callee");
  auto* dex_caller = marianatrench::redex::create_method(scope, "LCaller;", R"(
    (method (public) "LCaller;.caller:()V"
     (
      (load-param-object v0)
      (invoke-direct (v0) "LCalleeOne;.callee:()V")
      (load-param-object v1)
      (invoke-direct (v1) "LCalleeTwo;.callee:()V")
      (return-void)
     )
    )
  )");

  auto context = test_dependencies(scope);
  const auto& call_graph = *context.call_graph;
  const auto& dependencies = *context.dependencies;
  auto* callee_one = context.methods->get(dex_callee_one);
  auto* callee_two = context.methods->get(dex_callee_two);
  auto* caller = context.methods->get(dex_caller);

  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(caller)),
      testing::UnorderedElementsAre(callee_one, callee_two));
  EXPECT_TRUE(call_graph.callees(callee_one).empty());
  EXPECT_TRUE(call_graph.callees(callee_two).empty());

  EXPECT_TRUE(call_graph.artificial_callees(caller).empty());
  EXPECT_TRUE(call_graph.artificial_callees(callee_one).empty());
  EXPECT_TRUE(call_graph.artificial_callees(callee_two).empty());

  EXPECT_TRUE(dependencies.dependencies(caller).empty());
  EXPECT_THAT(
      dependencies.dependencies(callee_one),
      testing::UnorderedElementsAre(caller));
  EXPECT_THAT(
      dependencies.dependencies(callee_two),
      testing::UnorderedElementsAre(caller));
}

TEST_F(DependenciesTest, MultipleCallers) {
  Scope scope;

  auto* dex_callee =
      marianatrench::redex::create_void_method(scope, "LCallee;", "callee");
  auto* dex_caller_one = marianatrench::redex::create_method(
      scope,
      "LCallerOne;",
      R"(
      (method (public) "LCallerOne;.caller:()V"
       (
        (load-param-object v0)
        (invoke-direct (v0) "LCallee;.callee:()V")
        (return-void)
       )
      )
    )");
  auto* dex_caller_two = marianatrench::redex::create_method(
      scope,
      "LCallerTwo;",
      R"(
      (method (public) "LCallerTwo;.caller:()V"
       (
        (load-param-object v0)
        (invoke-direct (v0) "LCallee;.callee:()V")
        (return-void)
       )
      )
    )");

  auto context = test_dependencies(scope);
  const auto& call_graph = *context.call_graph;
  const auto& dependencies = *context.dependencies;
  auto* callee = context.methods->get(dex_callee);
  auto* caller_one = context.methods->get(dex_caller_one);
  auto* caller_two = context.methods->get(dex_caller_two);

  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(caller_one)),
      testing::UnorderedElementsAre(callee));
  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(caller_two)),
      testing::UnorderedElementsAre(callee));
  EXPECT_TRUE(call_graph.callees(callee).empty());

  EXPECT_TRUE(call_graph.artificial_callees(caller_one).empty());
  EXPECT_TRUE(call_graph.artificial_callees(caller_two).empty());
  EXPECT_TRUE(call_graph.artificial_callees(callee).empty());

  EXPECT_TRUE(dependencies.dependencies(caller_one).empty());
  EXPECT_TRUE(dependencies.dependencies(caller_two).empty());
  EXPECT_THAT(
      dependencies.dependencies(callee),
      testing::UnorderedElementsAre(caller_one, caller_two));
}

TEST_F(DependenciesTest, Transitive) {
  Scope scope;

  auto* dex_callee =
      marianatrench::redex::create_void_method(scope, "LCallee;", "callee");
  auto* dex_caller = marianatrench::redex::create_method(
      scope,
      "LCaller;",
      R"(
      (method (public) "LCaller;.caller:()V"
       (
        (load-param-object v0)
        (invoke-direct (v0) "LCallee;.callee:()V")
        (return-void)
       )
      )
    )");
  auto* dex_indirect = marianatrench::redex::create_method(
      scope,
      "LIndirect;",
      R"(
      (method (public) "LIndirect;.caller:()V"
       (
        (load-param-object v0)
        (invoke-direct (v0) "LCaller;.caller:()V")
        (return-void)
       )
      )
    )");

  auto context = test_dependencies(scope);
  const auto& call_graph = *context.call_graph;
  const auto& dependencies = *context.dependencies;
  auto* callee = context.methods->get(dex_callee);
  auto* caller = context.methods->get(dex_caller);
  auto* indirect = context.methods->get(dex_indirect);

  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(indirect)),
      testing::UnorderedElementsAre(caller));
  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(caller)),
      testing::UnorderedElementsAre(callee));
  EXPECT_TRUE(call_graph.callees(callee).empty());

  EXPECT_TRUE(call_graph.artificial_callees(indirect).empty());
  EXPECT_TRUE(call_graph.artificial_callees(caller).empty());
  EXPECT_TRUE(call_graph.artificial_callees(callee).empty());

  EXPECT_TRUE(dependencies.dependencies(indirect).empty());
  EXPECT_THAT(
      dependencies.dependencies(caller),
      testing::UnorderedElementsAre(indirect));
  EXPECT_THAT(
      dependencies.dependencies(callee), testing::UnorderedElementsAre(caller));
}

TEST_F(DependenciesTest, DependenciesWithInheritance) {
  Scope scope;

  auto* dex_callee =
      marianatrench::redex::create_void_method(scope, "LCallee;", "callee");
  auto* dex_override = marianatrench::redex::create_void_method(
      scope,
      "LSubclassOne;",
      "callee",
      /* parameter_types */ "",
      /* return_type */ "V",
      /* super */ dex_callee->get_class());
  auto* dex_caller = marianatrench::redex::create_method(
      scope,
      "LCaller;",
      R"(
      (method (public) "LCaller;.caller:()V"
       (
        (load-param-object v0)
        (invoke-direct (v0) "LCallee;.callee:()V")
        (return-void)
       )
      )
    )");

  auto context = test_dependencies(scope);
  const auto& call_graph = *context.call_graph;
  const auto& dependencies = *context.dependencies;
  auto* callee = context.methods->get(dex_callee);
  auto* override = context.methods->get(dex_override);
  auto* caller = context.methods->get(dex_caller);

  EXPECT_TRUE(call_graph.callees(callee).empty());
  EXPECT_TRUE(call_graph.callees(override).empty());
  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(caller)),
      testing::UnorderedElementsAre(callee));

  EXPECT_TRUE(call_graph.artificial_callees(callee).empty());
  EXPECT_TRUE(call_graph.artificial_callees(override).empty());
  EXPECT_TRUE(call_graph.artificial_callees(caller).empty());

  EXPECT_THAT(
      dependencies.dependencies(callee), testing::UnorderedElementsAre(caller));
  EXPECT_TRUE(dependencies.dependencies(override).empty());
  EXPECT_TRUE(dependencies.dependencies(caller).empty());
}

TEST_F(DependenciesTest, DependenciesWithParameterTypeOverrides) {
  Scope scope;

  auto* dex_override_callee = marianatrench::redex::create_void_method(
      scope, "LOverrideCallee;", "override_callee");

  auto* dex_callee = marianatrench::redex::create_void_method(
      scope,
      "LCallee;",
      "callee",
      /* parameter_types */ "LData;");
  auto* dex_override = marianatrench::redex::create_method(
      scope,
      "LSubclass;",
      R"(
      (method (public) "LSubclass;.callee:(LData;)V"
       (
        (invoke-direct (v0) "LOverrideCallee;.override_callee:()V")
        (return-void)
       )
      )
      )",
      /* super */ dex_callee->get_class());

  auto* dex_caller = marianatrench::redex::create_method(
      scope,
      "LCaller;",
      R"(
      (method (public) "LCaller;.caller:(LCallee;)V"
       (
        (load-param-object v0)
        (load-param-object v1)

        (new-instance "LAnonymous$1;")
        (move-result-object v2)

        (invoke-virtual (v1 v2) "LCallee;.callee:(LData;)V")
        (return-void)
       )
      )
    )");

  auto context = test_dependencies(scope);
  const auto& overrides = *context.overrides;
  const auto& call_graph = *context.call_graph;
  const auto& dependencies = *context.dependencies;

  ParameterTypeOverrides parameter_type_overrides{
      {0, DexType::get_type("LAnonymous$1;")},
  };
  auto* override_callee = context.methods->get(dex_override_callee);
  auto* callee = context.methods->get(dex_callee);
  auto* override = context.methods->get(dex_override);
  auto* caller = context.methods->get(dex_caller);
  auto* callee_with_type_overrides =
      context.methods->get(dex_callee, parameter_type_overrides);
  auto* override_with_type_overrides =
      context.methods->get(dex_override, parameter_type_overrides);

  EXPECT_THAT(overrides.get(callee), testing::UnorderedElementsAre(override));
  EXPECT_THAT(
      overrides.get(callee_with_type_overrides),
      testing::UnorderedElementsAre(override_with_type_overrides));

  EXPECT_TRUE(call_graph.callees(callee).empty());
  EXPECT_TRUE(call_graph.callees(callee_with_type_overrides).empty());
  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(override)),
      testing::UnorderedElementsAre(override_callee));
  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(override_with_type_overrides)),
      testing::UnorderedElementsAre(override_callee));
  EXPECT_TRUE(call_graph.callees(override_callee).empty());
  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(caller)),
      testing::UnorderedElementsAre(callee_with_type_overrides));

  EXPECT_TRUE(call_graph.artificial_callees(callee).empty());
  EXPECT_TRUE(
      call_graph.artificial_callees(callee_with_type_overrides).empty());
  EXPECT_TRUE(call_graph.artificial_callees(override).empty());
  EXPECT_TRUE(
      call_graph.artificial_callees(override_with_type_overrides).empty());
  EXPECT_TRUE(call_graph.artificial_callees(override_callee).empty());
  EXPECT_TRUE(call_graph.artificial_callees(caller).empty());

  EXPECT_TRUE(dependencies.dependencies(callee).empty());
  EXPECT_THAT(
      dependencies.dependencies(callee_with_type_overrides),
      testing::UnorderedElementsAre(caller));
  EXPECT_TRUE(dependencies.dependencies(override).empty());
  EXPECT_THAT(
      dependencies.dependencies(override_with_type_overrides),
      testing::UnorderedElementsAre(caller));
  EXPECT_THAT(
      dependencies.dependencies(override_callee),
      testing::UnorderedElementsAre(override, override_with_type_overrides));
  EXPECT_TRUE(dependencies.dependencies(caller).empty());
}

TEST_F(DependenciesTest, VirtualCallResolution) {
  using namespace std::string_literals;

  Scope scope;

  auto* dex_callee = marianatrench::redex::create_void_method(
      scope,
      "LCallee;",
      "callee",
      /* parameter_types */ "LData;",
      /* return_type */ "V");
  auto* dex_override_one = marianatrench::redex::create_void_method(
      scope,
      "LSubclassOne;",
      "callee",
      /* parameter_types */ "LData;",
      /* return_type */ "V",
      /* super */ dex_callee->get_class());
  auto* dex_override_two = marianatrench::redex::create_void_method(
      scope,
      "LSubclassTwo;",
      "callee",
      /* parameter_types */ "LData;",
      /* return_type */ "V",
      /* super */ dex_override_one->get_class());
  auto* dex_override_three = marianatrench::redex::create_void_method(
      scope,
      "LSubclassThree;",
      "callee",
      /* parameter_types */ "LData;",
      /* return_type */ "V",
      /* super */ dex_override_one->get_class());

  auto* dex_caller = marianatrench::redex::create_method(
      scope,
      "LCaller;",
      R"(
      (method (public) "LCaller;.caller:(LData;Z)V"
       (
        (load-param-object v1)
        (load-param-object v2)
        (if-eqz v2 :label)
        (new-instance "LSubclassOne;")
        (move-result-object v0)
        (goto :call)
        (:label)
        (new-instance "LSubclassTwo;")
        (move-result-object v0)
        (goto :call)
        (:call)
        (invoke-virtual (v0 v1) "LCallee;.callee:(LData;)V")
        (return-void)
       )
      )
    )");

  auto context = test_dependencies(scope);
  const auto& overrides = *context.overrides;
  const auto& call_graph = *context.call_graph;
  const auto& dependencies = *context.dependencies;

  auto* callee = context.methods->get(dex_callee);
  auto* override_one = context.methods->get(dex_override_one);
  auto* override_two = context.methods->get(dex_override_two);
  auto* override_three = context.methods->get(dex_override_three);
  auto* caller = context.methods->get(dex_caller);

  EXPECT_THAT(
      overrides.get(callee),
      testing::UnorderedElementsAre(
          override_one, override_two, override_three));
  EXPECT_THAT(
      overrides.get(override_one),
      testing::UnorderedElementsAre(override_two, override_three));
  EXPECT_TRUE(overrides.get(override_two).empty());
  EXPECT_TRUE(overrides.get(caller).empty());

  EXPECT_TRUE(call_graph.callees(callee).empty());
  EXPECT_TRUE(call_graph.callees(override_one).empty());
  EXPECT_TRUE(call_graph.callees(override_two).empty());
  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(caller)),
      testing::UnorderedElementsAre(override_one));

  EXPECT_TRUE(call_graph.artificial_callees(callee).empty());
  EXPECT_TRUE(call_graph.artificial_callees(override_one).empty());
  EXPECT_TRUE(call_graph.artificial_callees(override_two).empty());
  EXPECT_TRUE(call_graph.artificial_callees(caller).empty());

  EXPECT_TRUE(dependencies.dependencies(callee).empty());
  EXPECT_THAT(
      dependencies.dependencies(override_one),
      testing::UnorderedElementsAre(caller));
  EXPECT_THAT(
      dependencies.dependencies(override_two),
      testing::UnorderedElementsAre(caller));
  EXPECT_TRUE(dependencies.dependencies(override_three).empty());
  EXPECT_TRUE(dependencies.dependencies(caller).empty());
}

TEST_F(DependenciesTest, NoJoinVirtualOverrides) {
  using namespace std::string_literals;

  Scope scope;

  auto* dex_callee = marianatrench::redex::create_void_method(
      scope,
      "LCallee;",
      "callee",
      /* parameter_types */ "LData;",
      /* return_type */ "V");
  auto* dex_override_one = marianatrench::redex::create_void_method(
      scope,
      "LSubclassOne;",
      "callee",
      /* parameter_types */ "LData;",
      /* return_type */ "V",
      /* super */ dex_callee->get_class());
  auto* dex_override_two = marianatrench::redex::create_void_method(
      scope,
      "LSubclassTwo;",
      "callee",
      /* parameter_types */ "LData;",
      /* return_type */ "V",
      /* super */ dex_override_one->get_class());

  auto* dex_caller = marianatrench::redex::create_method(
      scope,
      "LCaller;",
      R"(
      (method (public) "LCaller;.caller:(LCallee;)V"
       (
        (load-param-object v1)
        (load-param-object v0)

        (invoke-virtual (v0 v1) "LCallee;.callee:(LData;)V")
        (return-void)
       )
      )
    )");

  auto context = test_dependencies(scope);

  auto* callee = context.methods->get(dex_callee);
  auto* override_one = context.methods->get(dex_override_one);
  auto* override_two = context.methods->get(dex_override_two);
  auto* caller = context.methods->get(dex_caller);

  auto registry = Registry(context);
  registry.set(
      Model(callee, context, /* modes */ Model::Mode::NoJoinVirtualOverrides));

  context.dependencies = std::make_unique<Dependencies>(
      *context.options,
      *context.heuristics,
      *context.methods,
      *context.overrides,
      *context.call_graph,
      registry);

  const auto& overrides = *context.overrides;
  const auto& call_graph = *context.call_graph;
  const auto& dependencies = *context.dependencies;

  EXPECT_THAT(
      overrides.get(callee),
      testing::UnorderedElementsAre(override_one, override_two));
  EXPECT_THAT(
      overrides.get(override_one), testing::UnorderedElementsAre(override_two));
  EXPECT_TRUE(overrides.get(override_two).empty());
  EXPECT_TRUE(overrides.get(caller).empty());

  EXPECT_TRUE(call_graph.callees(callee).empty());
  EXPECT_TRUE(call_graph.callees(override_one).empty());
  EXPECT_TRUE(call_graph.callees(override_two).empty());
  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(caller)),
      testing::UnorderedElementsAre(callee));

  EXPECT_TRUE(call_graph.artificial_callees(callee).empty());
  EXPECT_TRUE(call_graph.artificial_callees(override_one).empty());
  EXPECT_TRUE(call_graph.artificial_callees(override_two).empty());
  EXPECT_TRUE(call_graph.artificial_callees(caller).empty());

  EXPECT_THAT(
      dependencies.dependencies(callee), testing::UnorderedElementsAre(caller));
  EXPECT_TRUE(dependencies.dependencies(override_one).empty());
  EXPECT_TRUE(dependencies.dependencies(override_two).empty());
  EXPECT_TRUE(dependencies.dependencies(caller).empty());
}

TEST_F(DependenciesTest, SuperCallResolution) {
  using namespace std::string_literals;

  Scope scope;

  auto* dex_callee = marianatrench::redex::create_void_method(
      scope,
      "LCallee;",
      "callee",
      /* parameter_types */ "LData;",
      /* return_type */ "V");
  auto* dex_override_one = marianatrench::redex::create_method(
      scope,
      "LSubclassOne;",
      R"(
      (method (public) "LSubclassOne;.callee:(LData;)V"
       (
        (load-param-object v0)
        (load-param-object v1)
        (invoke-super (v0 v1) "LCallee;.callee:(LData;)V")
        (return-void)
       )
      )
    )",
      /* super */ dex_callee->get_class());
  auto* dex_override_two = marianatrench::redex::create_method(
      scope,
      "LSubclassTwo;",
      R"(
      (method (public) "LSubclassTwo;.callee:(LData;)V"
       (
        (load-param-object v0)
        (load-param-object v1)
        (invoke-super (v0 v1) "LSubclassOne;.callee:(LData;)V")
        (return-void)
       )
      )
    )",
      /* super */ dex_override_one->get_class());

  auto context = test_dependencies(scope);
  const auto& overrides = *context.overrides;
  const auto& call_graph = *context.call_graph;
  const auto& dependencies = *context.dependencies;

  auto* callee = context.methods->get(dex_callee);
  auto* override_one = context.methods->get(dex_override_one);
  auto* override_two = context.methods->get(dex_override_two);

  EXPECT_THAT(
      overrides.get(callee),
      testing::UnorderedElementsAre(override_one, override_two));
  EXPECT_THAT(
      overrides.get(override_one), testing::UnorderedElementsAre(override_two));
  EXPECT_TRUE(overrides.get(override_two).empty());

  EXPECT_TRUE(call_graph.callees(callee).empty());
  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(override_one)),
      testing::UnorderedElementsAre(callee));
  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(override_two)),
      testing::UnorderedElementsAre(override_one));

  EXPECT_TRUE(call_graph.artificial_callees(callee).empty());
  EXPECT_TRUE(call_graph.artificial_callees(override_one).empty());
  EXPECT_TRUE(call_graph.artificial_callees(override_two).empty());

  EXPECT_THAT(
      dependencies.dependencies(callee),
      testing::UnorderedElementsAre(override_one));
  EXPECT_THAT(
      dependencies.dependencies(override_one),
      testing::UnorderedElementsAre(override_two));
  EXPECT_TRUE(dependencies.dependencies(override_two).empty());
}

TEST_F(DependenciesTest, ArtificialCalleesInvoke) {
  Scope scope;

  auto* dex_callee = marianatrench::redex::create_void_method(
      scope,
      "LCallee;",
      "callee",
      /* parameter_types */ "LData;");

  auto dex_anonymous = marianatrench::redex::create_methods(
      scope,
      "LAnonymous$1;",
      {
          R"(
      (method (public) "LAnonymous$1;.anonymous_one:()V"
       (
        (return-void)
       )
      ))",
          R"(
      (method (public) "LAnonymous$1;.anonymous_two:(LData;)V"
       (
        (load-param-object v0)
        (load-param-object v1)
        (invoke-virtual (v0 v1) "LCallee;.callee:(LData;)V")
        (return-void)
       )
      ))",
      });
  auto* dex_anonymous_one = dex_anonymous.at(0);
  auto* dex_anonymous_two = dex_anonymous.at(1);

  auto* dex_thread_init = marianatrench::redex::create_void_method(
      scope,
      "LThread;",
      "<init>",
      /* parameter_types */ "LRunnable;");
  dex_thread_init->set_code(nullptr);

  auto* dex_caller = marianatrench::redex::create_method(
      scope,
      "LCaller;",
      R"(
      (method (public) "LCaller;.caller:()V"
       (
        (load-param-object v0)

        (new-instance "LAnonymous$1;")
        (move-result-object v1)

        (invoke-virtual (v0 v1) "LThread;.<init>:(LRunnable;)V")
        (return-void)
       )
      )
    )");

  auto context = test_dependencies(scope);
  const auto& call_graph = *context.call_graph;
  const auto& dependencies = *context.dependencies;

  auto* callee = context.methods->get(dex_callee);
  auto* anonymous_one = context.methods->get(dex_anonymous_one);
  auto* anonymous_two = context.methods->get(dex_anonymous_two);
  auto* thread_init = context.methods->get(dex_thread_init);
  auto* caller = context.methods->get(dex_caller);

  EXPECT_TRUE(call_graph.callees(callee).empty());
  EXPECT_TRUE(call_graph.callees(anonymous_one).empty());
  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(anonymous_two)),
      testing::UnorderedElementsAre(callee));
  EXPECT_TRUE(call_graph.callees(thread_init).empty());
  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(caller)),
      testing::UnorderedElementsAre(thread_init));

  EXPECT_TRUE(call_graph.artificial_callees(callee).empty());
  EXPECT_TRUE(call_graph.artificial_callees(anonymous_one).empty());
  EXPECT_TRUE(call_graph.artificial_callees(anonymous_two).empty());
  EXPECT_TRUE(call_graph.artificial_callees(thread_init).empty());
  EXPECT_EQ(call_graph.artificial_callees(caller).size(), 1);

  const auto* invoke = call_graph.artificial_callees(caller).begin()->first;
  EXPECT_EQ(
      call_graph.artificial_callees(caller).begin()->second,
      (ArtificialCallees{
          ArtificialCallee{
              /* call_target */
              CallTarget::direct_call(
                  invoke,
                  anonymous_one,
                  anonymous_one->parameter_type(0),
                  CallTarget::CallKind::AnonymousClass,
                  /* call_index */ 0),
              /* root_registers */ {{Root::argument(0), 1}},
              /* features */
              FeatureSet{context.feature_factory->get(
                  "via-anonymous-class-to-obscure")}},
          ArtificialCallee{
              /* call_target */
              CallTarget::direct_call(
                  invoke,
                  anonymous_two,
                  anonymous_two->parameter_type(0),
                  CallTarget::CallKind::AnonymousClass,
                  /* call_index */ 0),
              /* root_registers */ {{Root::argument(0), 1}},
              /* features */
              FeatureSet{context.feature_factory->get(
                  "via-anonymous-class-to-obscure")}},
      }));

  EXPECT_THAT(
      dependencies.dependencies(callee),
      testing::UnorderedElementsAre(anonymous_two));
  EXPECT_THAT(
      dependencies.dependencies(anonymous_one),
      testing::UnorderedElementsAre(caller));
  EXPECT_THAT(
      dependencies.dependencies(anonymous_two),
      testing::UnorderedElementsAre(caller));
  EXPECT_THAT(
      dependencies.dependencies(thread_init),
      testing::UnorderedElementsAre(caller));
  EXPECT_TRUE(dependencies.dependencies(caller).empty());
}

TEST_F(DependenciesTest, ArtificialCalleesIput) {
  Scope scope;

  auto* dex_callee = marianatrench::redex::create_void_method(
      scope,
      "LCallee;",
      "callee",
      /* parameter_types */ "LData;");

  auto dex_anonymous = marianatrench::redex::create_methods(
      scope,
      "LAnonymous$1;",
      {
          R"(
      (method (public) "LAnonymous$1;.anonymous_one:()V"
       (
        (return-void)
       )
      ))",
          R"(
      (method (public) "LAnonymous$1;.anonymous_two:(LData;)V"
       (
        (load-param-object v0)
        (load-param-object v1)
        (invoke-virtual (v0 v1) "LCallee;.callee:(LData;)V")
        (return-void)
       )
      ))",
      });
  auto* dex_anonymous_one = dex_anonymous.at(0);
  auto* dex_anonymous_two = dex_anonymous.at(1);

  auto* dex_task = marianatrench::redex::create_method(
      scope,
      "LTask;",
      R"(
      (method (public) "LTask;.<init>:()V"
       (
        (load-param-object v0)

        (new-instance "LAnonymous$1;")
        (move-result-object v1)

        (iput-object v1 v0 "LTask;.runnable:LRunnable;")
        (return-void)
       )
      )
    )");

  auto context = test_dependencies(scope);
  const auto& call_graph = *context.call_graph;
  const auto& dependencies = *context.dependencies;

  auto* callee = context.methods->get(dex_callee);
  auto* anonymous_one = context.methods->get(dex_anonymous_one);
  auto* anonymous_two = context.methods->get(dex_anonymous_two);
  auto* task = context.methods->get(dex_task);

  EXPECT_TRUE(call_graph.callees(callee).empty());
  EXPECT_TRUE(call_graph.callees(anonymous_one).empty());
  EXPECT_THAT(
      resolved_base_callees(call_graph.callees(anonymous_two)),
      testing::UnorderedElementsAre(callee));
  EXPECT_TRUE(call_graph.callees(task).empty());

  EXPECT_TRUE(call_graph.artificial_callees(callee).empty());
  EXPECT_TRUE(call_graph.artificial_callees(anonymous_one).empty());
  EXPECT_TRUE(call_graph.artificial_callees(anonymous_two).empty());
  EXPECT_EQ(call_graph.artificial_callees(task).size(), 1);

  const auto* iput = call_graph.artificial_callees(task).begin()->first;
  EXPECT_EQ(
      call_graph.artificial_callees(task).begin()->second,
      (ArtificialCallees{
          ArtificialCallee{
              /* call_target */
              CallTarget::direct_call(
                  iput,
                  anonymous_one,
                  anonymous_one->parameter_type(0),
                  CallTarget::CallKind::AnonymousClass,
                  /* call_index */ 0),
              /* root_registers */ {{Root::argument(0), 1}},
              /* features */
              FeatureSet{context.feature_factory->get(
                  "via-anonymous-class-to-field")}},
          ArtificialCallee{
              /* call_target */
              CallTarget::direct_call(
                  iput,
                  anonymous_two,
                  anonymous_two->parameter_type(0),
                  CallTarget::CallKind::AnonymousClass,
                  /* call_index */ 0),
              /* root_registers */ {{Root::argument(0), 1}},
              /* features */
              FeatureSet{context.feature_factory->get(
                  "via-anonymous-class-to-field")}},
      }));

  EXPECT_THAT(
      dependencies.dependencies(callee),
      testing::UnorderedElementsAre(anonymous_two));
  EXPECT_THAT(
      dependencies.dependencies(anonymous_one),
      testing::UnorderedElementsAre(task));
  EXPECT_THAT(
      dependencies.dependencies(anonymous_two),
      testing::UnorderedElementsAre(task));
  EXPECT_TRUE(dependencies.dependencies(task).empty());
}

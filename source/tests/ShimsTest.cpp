/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/MethodMappings.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/shim-generator/ShimGeneration.h>
#include <mariana-trench/shim-generator/Shims.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class ShimsTest : public test::Test {};

namespace {
Context test_types(const Scope& scope) {
  Context context;
  context.options = std::make_unique<Options>(
      /* models_path */ std::vector<std::string>{},
      /* field_models_path */ std::vector<std::string>{},
      /* literal_models_path */ std::vector<std::string>{},
      /* rules_path */ std::vector<std::string>{},
      /* lifecycles_path */ std::vector<std::string>{},
      /* shims_path */ std::vector<std::string>{},
      /* proguard_configuration_paths */ std::vector<std::string>{},
      /* sequential */ false,
      /* skip_source_indexing */ true,
      /* skip_analysis */ true,
      /* model_generators_configuration */
      std::vector<ModelGeneratorConfiguration>{},
      /* model_generator_search_paths */ std::vector<std::string>{},
      /* remove_unreachable_code */ false,
      /* emit_all_via_cast_features */ false,
      /* source_root_directory */ ".",
      /* enable_cross_component_analysis */ true);
  DexStore store("test_store");
  store.add_classes(scope);
  context.stores = {store};
  context.artificial_methods = std::make_unique<ArtificialMethods>(
      *context.kind_factory, context.stores);
  context.methods = std::make_unique<Methods>(context.stores);
  context.control_flow_graphs =
      std::make_unique<ControlFlowGraphs>(context.stores);
  context.types = std::make_unique<Types>(*context.options, context.stores);
  return context;
}

using SerializedMultimap =
    std::vector<std::pair<std::string, std::vector<std::string>>>;

SerializedMultimap serialize_target_classes_to_receive_points(
    const IntentRoutingAnalyzer::TargetClassesToReceivePointsMap&
        target_classes_to_receive_points) {
  SerializedMultimap serialized;
  for (const auto& [dex_type, receiving_methods] :
       UnorderedIterable(target_classes_to_receive_points)) {
    std::vector<std::string> serialized_methods;
    for (const auto& receiving_method : receiving_methods) {
      serialized_methods.push_back(receiving_method.method->show());
    }
    sort(serialized_methods.begin(), serialized_methods.end());
    serialized.emplace_back(dex_type->str(), std::move(serialized_methods));
  }
  sort(serialized.begin(), serialized.end());
  return serialized;
}

SerializedMultimap serialize_method_to_send_targets(
    const IntentRoutingAnalyzer::MethodToSendTargetsMap&
        method_to_send_targets) {
  SerializedMultimap serialized;
  for (const auto& [method, dex_types] :
       UnorderedIterable(method_to_send_targets)) {
    std::vector<std::string> serialized_types;
    for (const auto& dex_type : dex_types) {
      serialized_types.emplace_back(dex_type->str());
    }
    sort(serialized_types.begin(), serialized_types.end());
    serialized.emplace_back(method->show(), serialized_types);
  }
  sort(serialized.begin(), serialized.end());
  return serialized;
}

} // namespace

TEST_F(ShimsTest, TestBuildCrossComponentAnalysisShims) {
  Scope scope;
  auto intent_methods = marianatrench::redex::create_methods(
      scope,
      "Landroid/content/Intent;",
      {
          R"(
            (method (public) "Landroid/content/Intent;.<init>:(Landroid/content/Context;Ljava/lang/Class;)V"
            (
              (return-void)
            )
            ))",
          R"(
            (method (public) "Landroid/content/Intent;.<init>:(Ljava/lang/Class;)V"
            (
              (return-void)
            )
            ))",
          R"(
            (method (public) "Landroid/content/Intent;.setClass:(Landroid/content/Context;Ljava/lang/Class;)Landroid/content/Intent;"
            (
              (return-void)
            )
            ))",
      });
  auto dex_methods = marianatrench::redex::create_methods(
      scope,
      "LClass;",
      {
          R"(
            (method (public) "LClass;.routes_intent_via_constructor:()V"
            (
              (new-instance "Landroid/content/Intent;")
              (move-result-pseudo-object v0)
              (new-instance "Landroid/content/Context;")
              (move-result-pseudo-object v1)
              (const-class "LRouteTo;")
              (move-result-pseudo-object v2)
              (invoke-direct (v0 v1 v2) "Landroid/content/Intent;.<init>:(Landroid/content/Context;Ljava/lang/Class;)V")
              (return-void)
            )
            ))",
          R"(
            (method (public) "LClass;.routes_intent_via_set_class:()V"
            (
              (new-instance "Landroid/content/Intent;")
              (move-result-pseudo-object v0)
              (invoke-direct (v0) "Landroid/content/Intent;.<init>:()V")
              (new-instance "Landroid/content/Context;")
              (move-result-pseudo-object v1)
              (const-class "LRouteTo;")
              (move-result-pseudo-object v2)
              (invoke-direct (v0 v1 v2) "Landroid/content/Intent;.setClass:(Landroid/content/Context;Ljava/lang/Class;)Landroid/content/Intent;")
              (return-void)
            )
            ))",
          R"(
            (method (public) "LClass;.does_not_route_intent:()V"
            (
              (new-instance "Landroid/content/Intent;")
              (move-result-pseudo-object v0)
              (const-class "LDontRouteTo;")
              (move-result-pseudo-object v1)
              (invoke-direct (v0 v1) "Landroid/content/Intent;.<init>:(Ljava/lang/Class;)V")
              (return-void)
            )
            ))",
          R"(
            (method (public) "LClass;.getIntent:()Landroid/content/Intent;"
            (
              (new-instance "Landroid/content/Intent;")
              (move-result-pseudo-object v0)
              (return-object v0)
            )
            ))",
          R"(
            (method (public) "LClass;.gets_routed_intent:()V"
            (
              (new-instance "Landroid/content/Intent;")
              (move-result-pseudo-object v0)
              (invoke-direct (v0) "LClass;.getIntent:()Landroid/content/Intent;")
              (return-void)
            )
            ))",
          R"(
            (method (public) "LClass;.also_gets_routed_intent:()V"
            (
              (new-instance "Landroid/content/Intent;")
              (move-result-pseudo-object v0)
              (invoke-direct (v0) "LClass;.getIntent:()Landroid/content/Intent;")
              (return-void)
            )
            ))",
      });

  auto context = test_types(scope);
  auto intent_routing_analyzer = IntentRoutingAnalyzer::run(
      *context.methods, *context.types, *context.options);

  auto target_classes_to_receive_points =
      serialize_target_classes_to_receive_points(
          intent_routing_analyzer->target_classes_to_receive_points());
  SerializedMultimap expected_classes_to_intent_receivers{std::make_pair(
      "LClass;",
      std::vector<std::string>{
          "LClass;.also_gets_routed_intent:()V",
          "LClass;.gets_routed_intent:()V",
      })};
  EXPECT_EQ(
      target_classes_to_receive_points, expected_classes_to_intent_receivers);

  auto method_to_send_targets = serialize_method_to_send_targets(
      intent_routing_analyzer->method_to_send_targets());
  SerializedMultimap expected_method_to_send_targets{
      std::make_pair(
          "LClass;.routes_intent_via_constructor:()V",
          std::vector<std::string>{"LRouteTo;"}),
      std::make_pair(
          "LClass;.routes_intent_via_set_class:()V",
          std::vector<std::string>{"LRouteTo;"}),

  };
  EXPECT_EQ(method_to_send_targets, expected_method_to_send_targets);
}

TEST_F(ShimsTest, TestGetShimForCaller) {
  Scope scope;
  auto intent_methods = marianatrench::redex::create_methods(
      scope,
      "Landroid/content/Intent;",
      {
          R"(
            (method (public) "Landroid/content/Intent;.<init>:(Landroid/content/Context;Ljava/lang/Class;)V"
            (
              (return-void)
            )
            ))"});
  auto routing_class_methods = marianatrench::redex::create_methods(
      scope,
      "LClass;",
      {
          R"(
            (method (public) "LClass;.routes_intent:()V"
            (
              (new-instance "Landroid/content/Intent;")
              (move-result-pseudo-object v0)
              (new-instance "Landroid/content/Context;")
              (move-result-pseudo-object v1)
              (const-class "LRouteTo;")
              (move-result-pseudo-object v2)
              (invoke-direct (v0 v1 v2) "Landroid/content/Intent;.<init>:(Landroid/content/Context;Ljava/lang/Class;)V")
              (invoke-direct (v0) "LClass;.startActivity:(Landroid/content/Intent;)V")
              (return-void)
            )
            ))",
          R"(
            (method (public) "LClass;.startActivity:(Landroid/content/Intent;)V"
            (
              (return-void)
            )
            ))"});

  auto routed_class_methods = marianatrench::redex::create_methods(
      scope,
      "LRouteTo;",
      {
          R"(
            (method (public) "LRouteTo;.getIntent:()Landroid/content/Intent;"
            (
              (new-instance "Landroid/content/Intent;")
              (move-result-pseudo-object v0)
              (return-object v0)
            )
            ))",
          R"(
            (method (public) "LRouteTo;.gets_routed_intent:()V"
            (
              (new-instance "Landroid/content/Intent;")
              (move-result-pseudo-object v0)
              (invoke-direct (v0) "LRouteTo;.getIntent:()Landroid/content/Intent;")
              (return-void)
            )
            ))",
          R"(
            (method (public) "LRouteTo;.also_gets_routed_intent:()V"
            (
              (new-instance "Landroid/content/Intent;")
              (move-result-pseudo-object v0)
              (invoke-direct (v0) "LRouteTo;.getIntent:()Landroid/content/Intent;")
              (return-void)
            )
            ))",
      });

  auto context = test_types(scope);
  MethodMappings method_mappings = MethodMappings(*context.methods);
  auto intent_routing_analyzer = IntentRoutingAnalyzer::run(
      *context.methods, *context.types, *context.options);
  Shims shims = ShimGeneration::run(context, method_mappings);
  shims.add_intent_routing_analyzer(std::move(intent_routing_analyzer));

  const Method* route_intent = context.methods->get(routing_class_methods[0]);
  const Method* start_activity = context.methods->get(routing_class_methods[1]);
  auto found_shim = shims.get_shim_for_caller(
      /* original_callee */ start_activity, /* caller */ route_intent);
  ASSERT_TRUE(found_shim != std::nullopt);
  std::vector<std::string> serialized_routing_targets;
  for (const auto& shim_target : found_shim->intent_routing_targets()) {
    const auto& method_spec = shim_target.method_spec();
    serialized_routing_targets.push_back(
        fmt::format(
            "{}.{}:{}",
            show(method_spec.cls),
            show(method_spec.name),
            show(method_spec.proto)));
  }
  sort(serialized_routing_targets.begin(), serialized_routing_targets.end());
  EXPECT_EQ(
      serialized_routing_targets,
      (std::vector<std::string>{
          "LRouteTo;.also_gets_routed_intent:()V",
          "LRouteTo;.gets_routed_intent:()V"}));
}
} // namespace marianatrench

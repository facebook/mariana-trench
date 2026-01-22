/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <mariana-trench/IntentRoutingAnalyzer.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

using namespace marianatrench;

namespace {
class IntentRoutingAnalyzerTest : public test::Test {};

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

std::vector<const Method*> get_methods(
    Context& context,
    std::vector<DexMethod*> dex_methods) {
  // ReturnsThis requires the cfg to be built.
  for (auto* method : dex_methods) {
    method->get_code()->build_cfg();
  }

  std::vector<const Method*> methods;
  std::transform(
      dex_methods.begin(),
      dex_methods.end(),
      std::back_inserter(methods),
      [&context](DexMethod* dex_method) -> const Method* {
        return context.methods->create(dex_method);
      });

  return methods;
}

} // namespace

TEST_F(IntentRoutingAnalyzerTest, IntentRoutingConstraint) {
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
      });
  auto dex_methods = marianatrench::redex::create_methods(
      scope,
      "LClass;",
      {
          R"(
            (method (public) "LClass;.method_1:()V"
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
          // We only accept the context + class constructor for now.
          R"(
            (method (public) "LClass;.method_2:()V"
            (
              (new-instance "Landroid/content/Intent;")
              (move-result-pseudo-object v0)
              (const-class "LDontRouteTo;")
              (move-result-pseudo-object v1)
              (invoke-direct (v0 v1) "Landroid/content/Intent;.<init>:(Ljava/lang/Class;)V")
              (return-void)
            )
            ))",
      });

  auto context = test_types(scope);
  auto methods = get_methods(context, dex_methods);
  auto intent_routing_analyzer = IntentRoutingAnalyzer::run(
      *context.methods, *context.types, *context.options);

  const auto& method_to_send_targets =
      intent_routing_analyzer->method_to_send_targets();

  auto send_point_with_hit = method_to_send_targets.find(methods[0]);
  EXPECT_NE(send_point_with_hit, method_to_send_targets.end());
  EXPECT_EQ(send_point_with_hit->second.size(), 1);
  EXPECT_EQ(send_point_with_hit->second[0]->str(), "LRouteTo;");

  auto send_point_without_hit = method_to_send_targets.find(methods[1]);
  EXPECT_EQ(send_point_without_hit, method_to_send_targets.end());

  const auto& target_classes_to_receive_points =
      intent_routing_analyzer->target_classes_to_receive_points();
  EXPECT_EQ(target_classes_to_receive_points.size(), 0);
}

TEST_F(IntentRoutingAnalyzerTest, IntentRoutingSetClass) {
  Scope scope;
  auto intent_methods = marianatrench::redex::create_methods(
      scope,
      "Landroid/content/Intent;",
      {
          R"(
            (method (public) "Landroid/content/Intent;.<init>:()V"
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
            (method (public) "LClass;.method_1:()V"
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
      });

  auto context = test_types(scope);
  auto methods = get_methods(context, dex_methods);
  auto intent_routing_analyzer = IntentRoutingAnalyzer::run(
      *context.methods, *context.types, *context.options);

  const auto& method_to_send_targets =
      intent_routing_analyzer->method_to_send_targets();

  auto send_point = method_to_send_targets.find(methods[0]);
  EXPECT_NE(send_point, method_to_send_targets.end());
  EXPECT_EQ(send_point->second.size(), 1);
  EXPECT_EQ(send_point->second[0]->str(), "LRouteTo;");

  const auto& target_classes_to_receive_points =
      intent_routing_analyzer->target_classes_to_receive_points();
  EXPECT_EQ(target_classes_to_receive_points.size(), 0);
}

TEST_F(IntentRoutingAnalyzerTest, IntentRoutingGetIntent) {
  Scope scope;
  auto dex_methods = marianatrench::redex::create_methods(
      scope,
      "LClass;",
      {
          R"(
            (method (public) "LClass;.getIntent:()Landroid/content/Intent;"
            (
              (new-instance "Landroid/content/Intent;")
              (move-result-pseudo-object v0)
              (return-object v0)
            )
            ))",
          R"(
            (method (public) "LClass;.method_1:()V"
            (
              (new-instance "Landroid/content/Intent;")
              (move-result-pseudo-object v0)
              (invoke-direct (v0) "LClass;.getIntent:()Landroid/content/Intent;")
              (return-void)
            )
            ))",
      });

  auto context = test_types(scope);
  auto methods = get_methods(context, dex_methods);
  auto intent_routing_analyzer = IntentRoutingAnalyzer::run(
      *context.methods, *context.types, *context.options);

  const auto& method_to_send_targets =
      intent_routing_analyzer->method_to_send_targets();

  auto send_point = method_to_send_targets.find(methods[1]);
  EXPECT_EQ(send_point, method_to_send_targets.end());

  const auto& target_classes_to_receive_points =
      intent_routing_analyzer->target_classes_to_receive_points();
  EXPECT_EQ(target_classes_to_receive_points.size(), 1);

  auto intent_getters =
      target_classes_to_receive_points.find(methods[1]->get_class());
  EXPECT_NE(intent_getters, target_classes_to_receive_points.end());
  EXPECT_EQ(intent_getters->second.size(), 1);
  EXPECT_EQ(intent_getters->second[0].method, methods[1]);
}

TEST_F(IntentRoutingAnalyzerTest, IntentRoutingServiceIntent) {
  Scope scope;
  auto dex_methods = marianatrench::redex::create_methods(
      scope,
      "LClass;",
      {
          R"(
            (method (public) "LClass;.onStartCommand:(Landroid/content/Intent;II)I"
            (
              (return-void)
            )
            ))"});

  auto context = test_types(scope);
  auto methods = get_methods(context, dex_methods);
  auto intent_routing_analyzer = IntentRoutingAnalyzer::run(
      *context.methods, *context.types, *context.options);

  const auto& method_to_send_targets =
      intent_routing_analyzer->method_to_send_targets();

  auto send_point = method_to_send_targets.find(methods[0]);
  EXPECT_EQ(send_point, method_to_send_targets.end());

  const auto& target_classes_to_receive_points =
      intent_routing_analyzer->target_classes_to_receive_points();
  EXPECT_EQ(target_classes_to_receive_points.size(), 1);

  const auto intent_receivers =
      target_classes_to_receive_points.find(methods[0]->get_class());
  EXPECT_NE(intent_receivers, target_classes_to_receive_points.end());
  EXPECT_EQ(intent_receivers->second.size(), 1);
  EXPECT_EQ(intent_receivers->second[0].method, methods[0]);
}

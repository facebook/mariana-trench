/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/ClassProperties.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Dependencies.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/Registry.h>
#include <mariana-trench/Types.h>
#include <mariana-trench/UnusedKinds.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

namespace {

class RegistryTest : public test::Test {};

} // namespace

TEST_F(RegistryTest, remove_kinds) {
  Scope scope;
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  auto registry = Registry::load(
      context,
      *context.options,
      context.artificial_methods->models(
          context)); // used to make sure we get ArrayAllocation
  context.rules =
      std::make_unique<Rules>(Rules::load(context, *context.options));
  auto old_json = registry.models_to_json();
  auto unused_kinds = context.rules->collect_unused_kinds(*context.kinds);
  auto is_array_allocation = [](const Kind* kind) -> bool {
    const auto* named_kind = kind->as<NamedKind>();
    return named_kind != nullptr && named_kind->name() == "ArrayAllocation";
  };
  EXPECT_NE(
      std::find_if(
          unused_kinds.begin(), unused_kinds.end(), is_array_allocation),
      unused_kinds.end());
  EXPECT_TRUE(old_json[0].isMember("sinks"));
  EXPECT_EQ(old_json[0]["sinks"][0]["kind"], "ArrayAllocation");
  UnusedKinds::remove_unused_kinds(context, registry);
  EXPECT_NE(registry.models_to_json(), old_json);
  EXPECT_FALSE(registry.models_to_json()[0].isMember("sinks"));
}

TEST_F(RegistryTest, JoinWith) {
  Scope scope;
  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "LClass;",
      /* method_name */ "method",
      /* parameter_types */ "Ljava/lang/Object;",
      /* return_type */ "Ljava/lang/Object;");

  DexStore store("store");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  auto registry = Registry(context);
  registry.join_with(Registry(context, test::parse_json(R"([
          {
            "method": "LClass;.method:(Ljava/lang/Object;)Ljava/lang/Object;",
            "generations": [
              {
                "kind": "FirstSource",
                "port": "Return"
              }
            ]
          }
        ])")));

  auto model = Model();

  EXPECT_EQ(registry.get(method).generations().elements().size(), 1);
  EXPECT_EQ(
      registry.get(method).generations().elements().at(0).second.size(), 1);

  registry.join_with(Registry(context, test::parse_json(R"([
          {
            "method": "LClass;.method:(Ljava/lang/Object;)Ljava/lang/Object;",
            "generations": [
              {
                "kind": "SecondSource",
                "port": "Return"
              }
            ]
          }
        ])")));

  EXPECT_EQ(registry.get(method).generations().elements().size(), 1);
  EXPECT_EQ(
      registry.get(method).generations().elements().at(0).second.size(), 2);
}

TEST_F(RegistryTest, ConstructorUseJoin) {
  using PortTaint = std::pair<AccessPath, Taint>;

  Scope scope;
  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "LClass;",
      /* method_name */ "method",
      /* parameter_types */ "Ljava/lang/Object;Ljava/lang/Object;",
      /* return_type */ "Ljava/lang/Object;");

  DexStore store("store");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);
  const auto* source_kind = context.kinds->get("TestSource");

  Model model_with_source(
      /* method */ method,
      context,
      /* modes */ Model::Mode::Normal,
      /* generations */
      {{AccessPath(Root(Root::Kind::Return)), Frame::leaf(source_kind)}});

  Model model_with_other_source(
      /* method */ method,
      context,
      /* modes */ Model::Mode::Normal,
      /* generations */
      {{AccessPath(Root(Root::Kind::Argument, 2)), Frame::leaf(source_kind)}});

  auto registry =
      Registry(context, {model_with_source, model_with_other_source});
  EXPECT_THAT(
      registry.get(method).generations().elements(),
      testing::UnorderedElementsAre(
          PortTaint{
              AccessPath(Root(Root::Kind::Return)),
              Taint{Frame::leaf(
                  source_kind,
                  /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* user_features */ FeatureSet::bottom(),
                  /* origins */ MethodSet{method})}},
          PortTaint{
              AccessPath(Root(Root::Kind::Argument, 2)),
              Taint{Frame::leaf(
                  source_kind,
                  /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* user_features */ FeatureSet::bottom(),
                  /* origins */ MethodSet{method})}}));
}

} // namespace marianatrench

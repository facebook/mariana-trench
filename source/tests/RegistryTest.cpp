/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <json/value.h>
#include <mariana-trench/ClassProperties.h>
#include <mariana-trench/Constants.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Dependencies.h>
#include <mariana-trench/Fields.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/Registry.h>
#include <mariana-trench/Types.h>
#include <mariana-trench/UsedKinds.h>
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

  auto registry = Registry(
      context,
      /* method_models */
      context.artificial_methods->models(
          context), // used to make sure we get ArrayAllocation
      /* field_models */ {},
      /* literal_models */ {});

  context.rules =
      std::make_unique<Rules>(Rules::load(context, *context.options));
  context.used_kinds = std::make_unique<UsedKinds>(
      UsedKinds::from_rules(*context.rules, *context.transforms_factory));
  auto old_model_json = JsonValidation::null_or_array(
      registry.models_to_json(), /* field */ "models");
  auto unused_kinds =
      context.rules->collect_unused_kinds(*context.kind_factory);
  auto is_array_allocation = [](const Kind* kind) -> bool {
    const auto* named_kind = kind->as<NamedKind>();
    return named_kind != nullptr && named_kind->name() == "ArrayAllocation";
  };
  EXPECT_NE(
      std::find_if(
          unused_kinds.begin(), unused_kinds.end(), is_array_allocation),
      unused_kinds.end());
  EXPECT_TRUE(old_model_json[0].isMember("sinks"));
  // JSON model:
  // [ "sinks": [
  //   { "caller_port": "Argument(0)",
  //     "taint": [
  //       {
  //         "call" : { /* callee, port, position */ },
  //         "kinds" : [
  //           { /* Frame */ "kind": "ArrayAllocation", distance: 2, }
  //         ]
  //       } // end "taint[0]"
  //     ] // end "taint"
  //   }
  // ] ]
  EXPECT_EQ(
      old_model_json[0]["sinks"][0]["taint"][0]["kinds"][0]["kind"],
      "ArrayAllocation");
  context.used_kinds->remove_unused_kinds(
      *context.rules,
      *context.kind_factory,
      *context.methods,
      *context.artificial_methods,
      registry);
  auto new_model_json = JsonValidation::null_or_array(
      registry.models_to_json(), /* field */ "models");
  EXPECT_NE(new_model_json, old_model_json);
  EXPECT_FALSE(new_model_json[0].isMember("sinks"));
}

TEST_F(RegistryTest, JoinWith) {
  Scope scope;
  auto* dex_method = marianatrench::redex::create_void_method(
      scope,
      /* class_name */ "LClass;",
      /* method_name */ "method",
      /* parameter_types */ "Ljava/lang/Object;",
      /* return_type */ "Ljava/lang/Object;");
  const auto* dex_field = marianatrench::redex::create_field(
      scope, "LClassA;", {"field", type::java_lang_String()});

  DexStore store("store");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);
  auto field = context.fields->get(dex_field);
  const auto* source_kind = context.kind_factory->get("TestSource");
  const auto* source_kind_two = context.kind_factory->get("TestSourceTwo");

  auto registry = Registry(context);
  registry.join_with(Registry(
      context,
      /* models_value */ test::parse_json(R"([
          {
            "method": "LClass;.method:(Ljava/lang/Object;)Ljava/lang/Object;",
            "generations": [
              {
                "kind": "FirstSource",
                "port": "Return"
              }
            ]
          }
        ])"),
      /* field_models_value */ test::parse_json(R"([])"),
      /* literal_models_value */ test::parse_json(R"([])")));

  EXPECT_EQ(registry.get(method).generations().elements().size(), 1);
  EXPECT_EQ(
      registry.get(method).generations().elements().at(0).second.num_frames(),
      1);

  registry.join_with(Registry(
      context,
      /* models_value */ test::parse_json(R"([
          {
            "method": "LClass;.method:(Ljava/lang/Object;)Ljava/lang/Object;",
            "generations": [
              {
                "kind": "SecondSource",
                "port": "Return"
              }
            ]
          }
        ])"),
      /* field_models_value */ test::parse_json(R"([])"),
      /* literal_models_value */ test::parse_json(R"([])")));

  EXPECT_EQ(registry.get(method).generations().elements().size(), 1);
  EXPECT_EQ(
      registry.get(method).generations().elements().at(0).second.num_frames(),
      2);

  registry.join_with(Registry(
      context,
      /* models */ {},
      /* field_models */
      {FieldModel(
          field,
          /* sources */
          {test::make_taint_config(
              source_kind,
              test::FrameProperties{
                  .origins =
                      OriginSet{context.origin_factory->field_origin(field)},
                  .inferred_features = FeatureMayAlwaysSet::bottom(),
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .user_features = FeatureSet::bottom()})},
          /* sinks */ {})},
      /* literal_models */ {}));
  EXPECT_EQ(registry.get(field).sources().num_frames(), 1);

  registry.join_with(Registry(
      context,
      /* models */ {},
      /* field_models */
      {FieldModel(
          field,
          /* sources */
          {test::make_taint_config(
              source_kind_two,
              test::FrameProperties{
                  .origins =
                      OriginSet{context.origin_factory->field_origin(field)},
                  .inferred_features = FeatureMayAlwaysSet::bottom(),
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .user_features = FeatureSet::bottom()})},
          /* sinks */ {})},
      /* literal_models*/ {}));
  EXPECT_EQ(registry.get(field).sources().num_frames(), 2);
}

TEST_F(RegistryTest, ConstructorUseJoin) {
  using PortTaint = std::pair<AccessPath, Taint>;

  Scope scope;
  auto* dex_method = marianatrench::redex::create_void_method(
      scope,
      /* class_name */ "LClass;",
      /* method_name */ "method",
      /* parameter_types */ "Ljava/lang/Object;Ljava/lang/Object;",
      /* return_type */ "Ljava/lang/Object;");
  const auto* dex_field = marianatrench::redex::create_field(
      scope, "LClassA;", {"field", type::java_lang_String()});

  DexStore store("store");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);
  auto field = context.fields->get(dex_field);
  const auto* source_kind = context.kind_factory->get("TestSource");
  const auto* source_kind_two = context.kind_factory->get("TestSourceTwo");

  auto* return_port =
      context.access_path_factory->get(AccessPath(Root(Root::Kind::Return)));
  auto* argument2_port = context.access_path_factory->get(
      AccessPath(Root(Root::Kind::Argument, 2)));
  auto* return_origin =
      context.origin_factory->method_origin(method, return_port);
  auto* argument2_origin =
      context.origin_factory->method_origin(method, argument2_port);

  Model model_with_source(
      /* method */ method,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* config_overrides */ {},
      /* generations */
      {{AccessPath(Root(Root::Kind::Return)),
        test::make_leaf_taint_config(source_kind)}});

  Model model_with_other_source(
      /* method */ method,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* config_overrides */ {},
      /* generations */
      {{AccessPath(Root(Root::Kind::Argument, 2)),
        test::make_leaf_taint_config(source_kind)}});

  FieldModel field_with_source(
      /* field */ field,
      /* sources */ {test::make_leaf_taint_config(source_kind)},
      /* sinks */ {});

  FieldModel field_with_other_source(
      /* field */ field,
      /* sources */ {test::make_leaf_taint_config(source_kind_two)},
      /* sinks */ {});

  auto registry = Registry(
      context,
      /* models */ {model_with_source, model_with_other_source},
      /* field_models */ {field_with_source, field_with_other_source},
      /* literal_models */ std::vector<LiteralModel>{});
  EXPECT_THAT(
      registry.get(method).generations().elements(),
      testing::UnorderedElementsAre(
          PortTaint{
              *return_port,
              Taint{test::make_leaf_taint_config(
                  source_kind,
                  /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* locally_inferred_features */
                  FeatureMayAlwaysSet::bottom(),
                  /* user_features */ FeatureSet::bottom(),
                  /* origins */ OriginSet{return_origin})}},
          PortTaint{
              *argument2_port,
              Taint{test::make_leaf_taint_config(
                  source_kind,
                  /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* locally_inferred_features */
                  FeatureMayAlwaysSet::bottom(),
                  /* user_features */ FeatureSet::bottom(),
                  /* origins */ OriginSet{argument2_origin})}}));

  EXPECT_EQ(
      registry.get(field).sources(),
      Taint(
          {test::make_taint_config(
               source_kind,
               test::FrameProperties{
                   .origins =
                       OriginSet{context.origin_factory->field_origin(field)},
                   .inferred_features = FeatureMayAlwaysSet::bottom(),
                   .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                   .user_features = FeatureSet::bottom()}),
           test::make_taint_config(
               source_kind_two,
               test::FrameProperties{
                   .origins =
                       OriginSet{context.origin_factory->field_origin(field)},
                   .inferred_features = FeatureMayAlwaysSet::bottom(),
                   .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                   .user_features = FeatureSet::bottom()})}));
}

} // namespace marianatrench

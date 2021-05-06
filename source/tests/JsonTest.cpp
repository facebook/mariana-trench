/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/CanonicalName.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/LifecycleMethod.h>
#include <mariana-trench/LocalPositionSet.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/MultiSourceMultiSinkRule.h>
#include <mariana-trench/PartialKind.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/SourceSinkRule.h>
#include <mariana-trench/TriggeredPartialKind.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

namespace {

template <typename T, typename = decltype(std::declval<T>().to_json())>
Json::Value to_json(const T& value) {
  return value.to_json();
}

template <typename T, typename = decltype(std::declval<T>().to_json())>
Json::Value to_json(const T* value) {
  return value->to_json();
}

#define EXPECT_JSON_EQ(CLASS, JSON, EXPRESSION, ...)                           \
  do {                                                                         \
    EXPECT_EQ(                                                                 \
        CLASS::from_json(test::parse_json(JSON), ##__VA_ARGS__), EXPRESSION);  \
    EXPECT_EQ(test::parse_json(JSON), test::sorted_json(to_json(EXPRESSION))); \
    EXPECT_EQ(                                                                 \
        CLASS::from_json(to_json(EXPRESSION), ##__VA_ARGS__), EXPRESSION);     \
  } while (0)

} // namespace

class JsonTest : public test::Test {};

TEST_F(JsonTest, AccessPath) {
  const auto* x = DexString::make_string("x");
  const auto* y = DexString::make_string("y");
  const auto* z = DexString::make_string("z");

  EXPECT_THROW(
      AccessPath::from_json(test::parse_json("{}")), JsonValidationError);
  EXPECT_THROW(
      AccessPath::from_json(test::parse_json("1")), JsonValidationError);
  EXPECT_THROW(
      AccessPath::from_json(test::parse_json("\"Foo\"")), JsonValidationError);
  EXPECT_JSON_EQ(
      AccessPath, "\"Return\"", AccessPath(Root(Root::Kind::Return)));
  EXPECT_JSON_EQ(AccessPath, "\"Leaf\"", AccessPath(Root(Root::Kind::Leaf)));
  EXPECT_JSON_EQ(
      AccessPath, "\"Anchor\"", AccessPath(Root(Root::Kind::Anchor)));
  EXPECT_JSON_EQ(
      AccessPath, "\"Producer\"", AccessPath(Root(Root::Kind::Producer)));
  EXPECT_JSON_EQ(
      AccessPath, "\"Argument(0)\"", AccessPath(Root(Root::Kind::Argument, 0)));
  EXPECT_JSON_EQ(
      AccessPath, "\"Argument(1)\"", AccessPath(Root(Root::Kind::Argument, 1)));
  EXPECT_JSON_EQ(
      AccessPath,
      "\"Argument(12)\"",
      AccessPath(Root(Root::Kind::Argument, 12)));
  EXPECT_THROW(
      AccessPath::from_json(test::parse_json("\"Argument(0\"")),
      JsonValidationError);
  EXPECT_THROW(
      AccessPath::from_json(test::parse_json("\"Argument()\"")),
      JsonValidationError);
  EXPECT_THROW(
      AccessPath::from_json(test::parse_json("\"Argument(x)\"")),
      JsonValidationError);
  EXPECT_THROW(
      AccessPath::from_json(test::parse_json("\"Argument(x0)\"")),
      JsonValidationError);
  EXPECT_THROW(
      AccessPath::from_json(test::parse_json("\"Argument(0x)\"")),
      JsonValidationError);
  EXPECT_THROW(
      AccessPath::from_json(test::parse_json("\"Argument(-1)\"")),
      JsonValidationError);
  EXPECT_JSON_EQ(
      AccessPath,
      "\"Return.x\"",
      AccessPath(Root(Root::Kind::Return), Path{x}));
  EXPECT_JSON_EQ(
      AccessPath,
      "\"Return.x.y.z\"",
      AccessPath(Root(Root::Kind::Return), Path{x, y, z}));
  EXPECT_JSON_EQ(
      AccessPath,
      "\"Anchor.x\"",
      AccessPath(Root(Root::Kind::Anchor), Path{x}));
  EXPECT_JSON_EQ(
      AccessPath,
      "\"Producer.x\"",
      AccessPath(Root(Root::Kind::Producer), Path{x}));
  EXPECT_JSON_EQ(
      AccessPath,
      "\"Argument(1).x.y.z\"",
      AccessPath(Root(Root::Kind::Argument, 1), Path{x, y, z}));
}

TEST_F(JsonTest, Method) {
  Scope scope;
  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "LData;",
      /* method_name */ "method",
      /* parameter_types */ "LData;LData;",
      /* return_type*/ "V");
  redex::create_void_method(
      scope,
      /* class_name */ "LString;",
      /* method_name */ "<init>");
  redex::create_void_method(
      scope,
      /* class_name */ "LInteger;",
      /* method_name */ "<init>");

  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  EXPECT_THROW(
      Method::from_json(test::parse_json(R"({})"), context),
      JsonValidationError);
  EXPECT_THROW(
      Method::from_json(test::parse_json(R"(1)"), context),
      JsonValidationError);
  EXPECT_THROW(
      Method::from_json(
          test::parse_json(R"("LData;.non_existing:()V")"), context),
      JsonValidationError);
  EXPECT_JSON_EQ(Method, R"("LData;.method:(LData;LData;)V")", method, context);
  EXPECT_THROW(
      Method::from_json(
          test::parse_json(R"({"name": "LData;.non_existing:()V"})"), context),
      JsonValidationError);
  EXPECT_EQ(
      Method::from_json(
          test::parse_json(R"({"name": "LData;.method:(LData;LData;)V"})"),
          context),
      method);
  EXPECT_THROW(
      Method::from_json(
          test::parse_json(
              R"({
                "name": "LData;.method:(LData;LData;)V",
                "parameter_type_overrides": ""
              })"),
          context),
      JsonValidationError);
  EXPECT_EQ(
      Method::from_json(
          test::parse_json(
              R"({
                "name": "LData;.method:(LData;LData;)V",
                "parameter_type_overrides": [
                ]
              })"),
          context),
      method);
  EXPECT_THROW(
      Method::from_json(
          test::parse_json(
              R"({
                "name": "LData;.method:(LData;LData;)V",
                "parameter_type_overrides": [
                  {}
                ]
              })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Method::from_json(
          test::parse_json(
              R"({
                "name": "LData;.method:(LData;LData;)V",
                "parameter_type_overrides": [
                  {
                    "parameter": "x"
                  }
                ]
              })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Method::from_json(
          test::parse_json(
              R"({
                "name": "LData;.method:(LData;LData;)V",
                "parameter_type_overrides": [
                  {
                    "parameter": 1,
                    "type": 2
                  }
                ]
              })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Method::from_json(
          test::parse_json(
              R"({
                "name": "LData;.method:(LData;LData;)V",
                "parameter_type_overrides": [
                  {
                    "parameter": 1,
                    "type": "LNonExisting;"
                  }
                ]
              })"),
          context),
      JsonValidationError);
  EXPECT_JSON_EQ(
      Method,
      R"({
        "name": "LData;.method:(LData;LData;)V",
        "parameter_type_overrides": [
          {
            "parameter": 0,
            "type": "LString;"
          }
        ]
      })",
      context.methods->create(dex_method, {{0, redex::get_type("LString;")}}),
      context);
  EXPECT_JSON_EQ(
      Method,
      R"({
        "name": "LData;.method:(LData;LData;)V",
        "parameter_type_overrides": [
          {
            "parameter": 0,
            "type": "LString;"
          },
          {
            "parameter": 1,
            "type": "LInteger;"
          }
        ]
      })",
      context.methods->create(
          dex_method,
          {{0, redex::get_type("LString;")},
           {1, redex::get_type("LInteger;")}}),
      context);
}

TEST_F(JsonTest, Position) {
  Scope scope;
  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "LData;",
      /* method_name */ "method",
      /* parameter_types */ "LData;LData;",
      /* return_type*/ "V");
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_JSON_EQ(Position, R"({})", context.positions->unknown(), context);
  EXPECT_THROW(
      Position::from_json(test::parse_json(R"({"line": ""})"), context),
      JsonValidationError);
  EXPECT_THROW(
      Position::from_json(
          test::parse_json(R"({"line": 3, "start": "2"})"), context),
      JsonValidationError);
  EXPECT_JSON_EQ(
      Position,
      R"({"line": 1})",
      context.positions->get(/* path */ std::nullopt, 1),
      context);
  EXPECT_THROW(
      Position::from_json(
          test::parse_json(R"({"line": 1, "path": 2})"), context),
      JsonValidationError);
  EXPECT_JSON_EQ(
      Position,
      R"({"line": 2, "path": "Object.java"})",
      context.positions->get(/* path */ "Object.java", 2),
      context);
  EXPECT_JSON_EQ(
      Position,
      R"({"line": 2, "path": "Data.java"})",
      context.positions->get(dex_method, 2),
      context);
  EXPECT_JSON_EQ(
      Position,
      R"({"line": 2, "path": "Data.java", "start": 2, "end": 7})",
      context.positions->get(
          /* method */ dex_method,
          /* line */ 2,
          /* port */ std::nullopt,
          /* instruction */ nullptr,
          /* start */ 2,
          /* end */ 7),
      context);
}

TEST_F(JsonTest, Rule) {
  auto context = test::make_empty_context();

  EXPECT_THROW(
      Rule::from_json(
          test::parse_json(R"({
            "name": 1,
            "description": "rule_description",
            "sources": ["rule_source"],
            "sinks": ["rule_sink"]
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Rule::from_json(
          test::parse_json(R"({
            "name": "rule_name",
            "description": 1,
            "sources": ["rule_source"],
            "sinks": ["rule_sink"]
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Rule::from_json(
          test::parse_json(R"({
            "name": "rule_name",
            "description": "rule_description",
            "sources": [],
            "sinks": ["rule_sink"]
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Rule::from_json(
          test::parse_json(R"({
            "name": "rule_name",
            "description": "rule_description",
            "sources": [1],
            "sinks": ["rule_sink"]
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Rule::from_json(
          test::parse_json(R"({
            "name": "rule_name",
            "description": "rule_description",
            "sources": ["rule_source"],
            "sinks": []
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Rule::from_json(
          test::parse_json(R"({
            "name": "rule_name",
            "code": 1,
            "description": "rule_description",
            "sources": ["rule_source"],
            "sinks": []
          })"),
          context),
      JsonValidationError);

  /* Rule kind determination fails in the following cases. */
  EXPECT_THROW(
      Rule::from_json(
          test::parse_json(R"({
            "name": "rule_name",
            "code": 1,
            "description": "rule_description",
            "sources": ["rule_source"]
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Rule::from_json(
          test::parse_json(R"({
            "name": "rule_name",
            "code": 1,
            "description": "rule_description",
            "sinks": ["rule_sink"]
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Rule::from_json(
          test::parse_json(R"({
            "name": "rule_name",
            "code": 1,
            "description": "rule_description",
            "multi_sources": {
              "labelA": ["rule_source"],
              "labelB": ["rule_source"]
            }
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Rule::from_json(
          test::parse_json(R"({
            "name": "rule_name",
            "code": 1,
            "description": "rule_description",
            "partial_sinks": ["rule_sink"]
          })"),
          context),
      JsonValidationError);

  /* Multi source rules need exactly 2 labels. */
  EXPECT_THROW(
      Rule::from_json(
          test::parse_json(R"({
            "name": "rule_name",
            "code": 1,
            "description": "rule_description",
            "multi_sources": {
              "labelA": ["rule_source"]
            },
            "partial_sinks": ["rule_sink"]
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Rule::from_json(
          test::parse_json(R"({
            "name": "rule_name",
            "code": 1,
            "description": "rule_description",
            "multi_sources": {
              "labelA": ["rule_source"],
              "labelB": ["rule_source"],
              "labelC": ["rule_source"]
            },
            "partial_sinks": ["rule_sink"]
          })"),
          context),
      JsonValidationError);

  auto rule = Rule::from_json(
      test::parse_json(R"({
        "name": "rule_name",
        "code": 1,
        "description": "rule_description",
        "sources": ["rule_source"],
        "sinks": ["rule_sink"]
      })"),
      context);
  EXPECT_EQ(rule->as<MultiSourceMultiSinkRule>(), nullptr);
  auto rule_with_single_source_and_sink = rule->as<SourceSinkRule>();
  EXPECT_NE(rule_with_single_source_and_sink, nullptr);
  EXPECT_EQ(rule_with_single_source_and_sink->name(), "rule_name");
  EXPECT_EQ(rule_with_single_source_and_sink->code(), 1);
  EXPECT_EQ(
      rule_with_single_source_and_sink->description(), "rule_description");
  EXPECT_THAT(
      rule_with_single_source_and_sink->source_kinds(),
      testing::ElementsAre(context.kinds->get("rule_source")));
  EXPECT_THAT(
      rule_with_single_source_and_sink->sink_kinds(),
      testing::ElementsAre(context.kinds->get("rule_sink")));

  rule = Rule::from_json(
      test::parse_json(R"({
        "name": "rule_name",
        "code": 1,
        "description": "rule_description",
        "sources": ["rule_source_one", "rule_source_two"],
        "sinks": ["rule_sink_one", "rule_sink_two"]
      })"),
      context);
  EXPECT_EQ(rule->as<MultiSourceMultiSinkRule>(), nullptr);
  auto rule_with_multiple_sources_and_sinks = rule->as<SourceSinkRule>();
  EXPECT_NE(rule_with_multiple_sources_and_sinks, nullptr);
  EXPECT_THAT(
      rule_with_multiple_sources_and_sinks->source_kinds(),
      testing::UnorderedElementsAre(
          context.kinds->get("rule_source_one"),
          context.kinds->get("rule_source_two")));
  EXPECT_THAT(
      rule_with_multiple_sources_and_sinks->sink_kinds(),
      testing::UnorderedElementsAre(
          context.kinds->get("rule_sink_one"),
          context.kinds->get("rule_sink_two")));

  rule = Rule::from_json(
      test::parse_json(R"({
            "name": "rule_name",
            "code": 1,
            "description": "rule_description",
            "multi_sources": {
              "labelA": ["rule_source_one", "rule_source_two"],
              "labelB": ["rule_source_one"]
            },
            "partial_sinks": ["rule_sink"]
          })"),
      context);
  EXPECT_EQ(rule->as<SourceSinkRule>(), nullptr);
  auto rule_with_combined_sources = rule->as<MultiSourceMultiSinkRule>();
  EXPECT_NE(rule_with_combined_sources, nullptr);
  EXPECT_THAT(
      rule_with_combined_sources->partial_sink_kinds("labelA"),
      testing::UnorderedElementsAre(
          context.kinds->get_partial("rule_sink", "labelA")));
  EXPECT_THAT(
      rule_with_combined_sources->partial_sink_kinds("labelB"),
      testing::UnorderedElementsAre(
          context.kinds->get_partial("rule_sink", "labelB")));
  EXPECT_TRUE(rule_with_combined_sources->partial_sink_kinds("labelC").empty());
  const auto& multi_sources = rule_with_combined_sources->multi_source_kinds();
  EXPECT_THAT(
      multi_sources.find("labelA")->second,
      testing::UnorderedElementsAre(
          context.kinds->get("rule_source_one"),
          context.kinds->get("rule_source_two")));
  EXPECT_THAT(
      multi_sources.find("labelB")->second,
      testing::UnorderedElementsAre(context.kinds->get("rule_source_one")));
}

TEST_F(JsonTest, LocalPositionSet) {
  auto context = test::make_empty_context();

  EXPECT_THROW(
      LocalPositionSet::from_json(test::parse_json(R"({})"), context),
      JsonValidationError);
  EXPECT_JSON_EQ(LocalPositionSet, R"([])", LocalPositionSet{}, context);
  EXPECT_JSON_EQ(
      LocalPositionSet,
      R"([{"line": 10}])",
      LocalPositionSet{context.positions->get(/* path */ std::nullopt, 10)},
      context);
  EXPECT_JSON_EQ(
      LocalPositionSet,
      R"([{"line": 10}, {"line": 20}])",
      (LocalPositionSet{
          context.positions->get(/* path */ std::nullopt, 10),
          context.positions->get(/* path */ std::nullopt, 20),
      }),
      context);
  EXPECT_EQ(
      test::sorted_json((LocalPositionSet{
                             context.positions->get(/* path */ "Test.java", 1),
                             context.positions->get(/* path */ "Test.java", 2),
                         })
                            .to_json()),
      test::parse_json(R"#([{"line": 1}, {"line": 2}])#"));
}

TEST_F(JsonTest, Frame) {
  Scope scope;
  auto* dex_source_one =
      redex::create_void_method(scope, "LClassOne;", "source");
  auto* dex_source_two =
      redex::create_void_method(scope, "LClassTwo;", "source");

  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* source_one = context.methods->get(dex_source_one);
  auto* source_two = context.methods->get(dex_source_two);

  EXPECT_THROW(
      Frame::from_json(test::parse_json(R"(1)"), context), JsonValidationError);
  EXPECT_THROW(
      Frame::from_json(test::parse_json(R"({})"), context),
      JsonValidationError);

  // Parse the kind.
  EXPECT_THROW(
      Frame::from_json(test::parse_json(R"({"kind": 1})"), context),
      JsonValidationError);
  EXPECT_EQ(
      Frame::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource"
              })"),
          context),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}));

  // Parse the kind for partial leaves.
  EXPECT_THROW(
      Frame::from_json(
          test::parse_json(R"({"kind": "TestSink", "partial_label": 1})"),
          context),
      JsonValidationError);
  auto frame = Frame::from_json(
      test::parse_json(
          R"({
                "kind": "TestSink",
                "partial_label": "X"
              })"),
      context);
  EXPECT_EQ(
      frame,
      Frame(
          /* kind */ context.kinds->get_partial("TestSink", "X"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}));
  const auto* frame_kind = frame.kind()->as<PartialKind>();
  EXPECT_NE(frame_kind, nullptr);
  EXPECT_EQ(frame_kind->name(), "TestSink");
  EXPECT_EQ(frame_kind->label(), "X");
  EXPECT_EQ(frame_kind->as<TriggeredPartialKind>(), nullptr);

  // Parse the callee port.
  EXPECT_THROW(
      Frame::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "callee_port": "InvalidPort"
          })"),
          context),
      JsonValidationError);
  EXPECT_JSON_EQ(
      Frame,
      R"({
        "kind": "TestSource",
        "callee_port": "Leaf"
      })",
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      context);
  EXPECT_JSON_EQ(
      Frame,
      R"({
        "kind": "TestSource",
        "callee_port": "Anchor.x"
      })",
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */
          AccessPath(
              Root(Root::Kind::Anchor), Path{DexString::make_string("x")}),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      context);

  // Parse the callee, position and distance.
  EXPECT_JSON_EQ(
      Frame,
      R"({
        "kind": "TestSource",
        "callee_port": "Return",
        "callee": "LClassOne;.source:()V",
        "call_position": {},
        "distance": 1
      })",
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ source_one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ {},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      context);
  EXPECT_JSON_EQ(
      Frame,
      R"({
        "kind": "TestSource",
        "callee_port": "Return",
        "callee": "LClassOne;.source:()V",
        "call_position": {"line": 2, "path": "Object.java"},
        "distance": 2
      })",
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ source_one,
          /* call_position */ context.positions->get("Object.java", 2),
          /* distance */ 2,
          /* origins */ {},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      context);

  // Parse the origins.
  EXPECT_THROW(
      Frame::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "origins": "LClassOne;.source:()V"
          })"),
          context),
      JsonValidationError);
  EXPECT_JSON_EQ(
      Frame,
      R"({
        "kind": "TestSource",
        "callee_port": "Leaf",
        "origins": ["LClassOne;.source:()V"]
      })",
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ MethodSet{source_one},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      context);
  EXPECT_JSON_EQ(
      Frame,
      R"({
        "kind": "TestSource",
        "callee_port": "Leaf",
        "origins": ["LClassOne;.source:()V", "LClassTwo;.source:()V"]
      })",
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ MethodSet{source_one, source_two},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      context);

  // Parse the features.
  EXPECT_JSON_EQ(
      Frame,
      R"({
        "kind": "TestSource",
        "callee_port": "Leaf",
        "always_features": ["FeatureOne"]
      })",
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */
          FeatureMayAlwaysSet{context.features->get("FeatureOne")},
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      context);
  EXPECT_JSON_EQ(
      Frame,
      R"({
        "kind": "TestSource",
        "callee_port": "Leaf",
        "always_features": ["FeatureOne", "FeatureTwo"]
      })",
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */
          FeatureMayAlwaysSet{
              context.features->get("FeatureOne"),
              context.features->get("FeatureTwo")},
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      context);
  EXPECT_JSON_EQ(
      Frame,
      R"({
        "kind": "TestSource",
        "callee_port": "Leaf",
        "may_features": ["FeatureOne"],
        "always_features": ["FeatureTwo"]
      })",
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */
          FeatureMayAlwaysSet(
              /* may */ FeatureSet{context.features->get("FeatureOne")},
              /* always */ FeatureSet{context.features->get("FeatureTwo")}),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      context);
  EXPECT_JSON_EQ(
      Frame,
      R"({
        "kind": "TestSource",
        "callee_port": "Leaf",
        "may_features": ["FeatureOne"]
      })",
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */
          FeatureMayAlwaysSet(
              /* may */ FeatureSet{context.features->get("FeatureOne")},
              /* always */ FeatureSet{}),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      context);
  EXPECT_JSON_EQ(
      Frame,
      R"({
        "kind": "TestSource",
        "callee_port": "Leaf",
        "may_features": ["FeatureOne", "FeatureTwo"]
      })",
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */
          FeatureMayAlwaysSet(
              /* may */
              FeatureSet{
                  context.features->get("FeatureOne"),
                  context.features->get("FeatureTwo")},
              /* always */ FeatureSet{}),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      context);
  EXPECT_EQ(
      Frame::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "features": ["FeatureOne"]
              })"),
          context),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */
          FeatureSet{context.features->get("FeatureOne")},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}));
  EXPECT_EQ(
      Frame::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "features": ["FeatureOne", "FeatureTwo"]
              })"),
          context),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */
          FeatureSet{
              context.features->get("FeatureOne"),
              context.features->get("FeatureTwo")},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}));
  EXPECT_EQ(
      Frame::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "features": ["FeatureOne"],
                "may_features": ["FeatureTwo"]
              })"),
          context),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */
          FeatureMayAlwaysSet(
              /* may */ FeatureSet{context.features->get("FeatureTwo")},
              /* always */ FeatureSet{}),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ FeatureSet{context.features->get("FeatureOne")},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}));
  EXPECT_EQ(
      Frame::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "features": ["FeatureOne"],
                "may_features": ["FeatureTwo"],
                "always_features": ["FeatureThree"]
              })"),
          context),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */
          FeatureMayAlwaysSet(
              /* may */ FeatureSet{context.features->get("FeatureTwo")},
              /* always */
              FeatureSet{context.features->get("FeatureThree")}),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ FeatureSet{context.features->get("FeatureOne")},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}));
  EXPECT_EQ(
      Frame::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "features": ["FeatureOne"],
                "may_features": [],
                "always_features": []
              })"),
          context),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ {},
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */
          FeatureSet{context.features->get("FeatureOne")},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}));

  // Parse via_type_of_ports
  EXPECT_JSON_EQ(
      Frame,
      R"#({
        "kind": "TestSource",
        "callee_port": "Leaf",
        "via_type_of": ["Argument(1)", "Return"]
      })#",
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */
          RootSetAbstractDomain(
              {Root(Root::Kind::Return), Root(Root::Kind::Argument, 1)}),
          /* local_positions */ {},
          /* canonical_names */ {}),
      context);

  // Parse local positions.
  EXPECT_JSON_EQ(
      Frame,
      R"({
        "kind": "TestSource",
        "callee_port": "Leaf",
        "local_positions": [{"line": 1}]
      })",
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */
          LocalPositionSet{context.positions->get(std::nullopt, 1)},
          /* canonical_names */ {}),
      context);
  EXPECT_JSON_EQ(
      Frame,
      R"({
        "kind": "TestSource",
        "callee_port": "Leaf",
        "local_positions": [
          {"line": 10},
          {"line": 20},
          {"line": 30}
        ]
      })",
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */
          LocalPositionSet{
              context.positions->get(std::nullopt, 10),
              context.positions->get(std::nullopt, 20),
              context.positions->get(std::nullopt, 30),
          },
          /* canonical_names */ {}),
      context);
  EXPECT_EQ(
      Frame::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "local_positions": [
                  {"line": 1},
                  {"line": 2},
                  {"line": 1},
                  {"line": 2}
                ]
              })"),
          context),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */
          LocalPositionSet{
              context.positions->get(std::nullopt, 1),
              context.positions->get(std::nullopt, 2),
          },
          /* canonical_names */ {}));

  // Verifies to_json behavior for local inferred features. These cannot be
  // covered by from_json tests as they are never specified in json. Note that
  // locally_inferred_features show up twice in the json, once within a
  // "local_features" key, another as "may/always_features" in the object
  // alongside any existing inferred features.
  EXPECT_EQ(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */
          FeatureMayAlwaysSet::make_always(
              {context.features->get("FeatureTwo")}),
          /* locally_inferred_features */
          FeatureMayAlwaysSet(
              /* may */ FeatureSet{context.features->get("FeatureOne")},
              /* always */ {}),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})
          .to_json(),
      test::parse_json(R"({
        "kind": "TestSource",
        "callee_port": "Leaf",
        "may_features": ["FeatureOne"],
        "always_features": ["FeatureTwo"],
        "local_features": {
          "may_features": ["FeatureOne"]
        }
      })"));

  // Consistency checks.
  EXPECT_THROW(
      Frame::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "callee_port": "Return"
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Frame::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "call_position": {}
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Frame::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "distance": 1
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Frame::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "callee_port": "Leaf",
            "callee": "LClassOne;.source:()V"
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Frame::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "callee_port": "Return",
            "callee": "LClassOne;.source:()V"
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Frame::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "callee_port": "Return",
            "callee": "LClassOne;.source:()V",
            "call_position": {}
          })"),
          context),
      JsonValidationError);
}

TEST_F(JsonTest, Frame_Crtex) {
  Scope scope;
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_THROW(
      Frame::from_json(test::parse_json(R"(1)"), context), JsonValidationError);
  EXPECT_THROW(
      Frame::from_json(test::parse_json(R"({})"), context),
      JsonValidationError);

  // canonical_names cannot be empty.
  EXPECT_THROW(
      Frame::from_json(
          test::parse_json(R"({"kind": "TestSource", "canonical_names": []})"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Frame::from_json(
          test::parse_json(
              R"({"kind": "TestSource", "canonical_names": [ { "irrelevant": "field" } ]})"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Frame::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "canonical_names": [ {"template": "%programmatic_leaf_name%", "instantiated": "MyMethod::MyClass"} ]
              })"),
          context),
      JsonValidationError);
  EXPECT_EQ(
      Frame::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "canonical_names": [ {"template": "%programmatic_leaf_name%"} ]
              })"),
          context),
      Frame::crtex_leaf(
          context.kinds->get("TestSource"),
          /* origins */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* canonical_names */
          CanonicalNameSetAbstractDomain{CanonicalName(
              CanonicalName::TemplateValue{"%programmatic_leaf_name%"})}));
  EXPECT_EQ(
      Frame::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "canonical_names": [ {"instantiated": "Lcom/android/MyClass;.MyMethod"} ]
              })"),
          context),
      Frame::crtex_leaf(
          context.kinds->get("TestSource"),
          /* origins */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* canonical_names */
          CanonicalNameSetAbstractDomain{
              CanonicalName(CanonicalName::InstantiatedValue{
                  "Lcom/android/MyClass;.MyMethod"})}));
}

TEST_F(JsonTest, Propagation) {
  auto context = test::make_empty_context();

  EXPECT_THROW(
      Propagation::from_json(test::parse_json(R"({})"), context),
      JsonValidationError);
  EXPECT_THROW(
      Propagation::from_json(test::parse_json(R"#({"input": 1})#"), context),
      JsonValidationError);
  EXPECT_THROW(
      Propagation::from_json(test::parse_json(R"#({"input": "x"})#"), context),
      JsonValidationError);

  EXPECT_JSON_EQ(
      Propagation,
      R"#({"input": "Argument(1)"})#",
      Propagation(
          /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {}),
      context);

  EXPECT_JSON_EQ(
      Propagation,
      R"#({"input": "Argument(2)"})#",
      Propagation(
          /* input */ AccessPath(Root(Root::Kind::Argument, 2)),
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {}),
      context);

  EXPECT_JSON_EQ(
      Propagation,
      R"#({
        "input": "Argument(1)",
        "always_features": ["FeatureOne"]
      })#",
      Propagation(
          /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* inferred_features */
          FeatureMayAlwaysSet{context.features->get("FeatureOne")},
          /* user_features */ {}),
      context);
  EXPECT_JSON_EQ(
      Propagation,
      R"#({
        "input": "Argument(1)",
        "always_features": ["FeatureOne", "FeatureTwo"]
      })#",
      Propagation(
          /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* inferred_features */
          FeatureMayAlwaysSet{
              context.features->get("FeatureOne"),
              context.features->get("FeatureTwo")},
          /* user_features */ {}),
      context);
  EXPECT_JSON_EQ(
      Propagation,
      R"#({
        "input": "Argument(1)",
        "may_features": ["FeatureOne"],
        "always_features": ["FeatureTwo"]
      })#",
      Propagation(
          /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* inferred_features */
          FeatureMayAlwaysSet(
              /* may */ FeatureSet{context.features->get("FeatureOne")},
              /* always */ FeatureSet{context.features->get("FeatureTwo")}),
          /* user_features */ {}),
      context);
  EXPECT_JSON_EQ(
      Propagation,
      R"#({
        "input": "Argument(1)",
        "may_features": ["FeatureOne"]
      })#",
      Propagation(
          /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* inferred_features */
          FeatureMayAlwaysSet(
              /* may */ FeatureSet{context.features->get("FeatureOne")},
              /* always */ FeatureSet{}),
          /* user_features */ {}),
      context);
  EXPECT_EQ(
      Propagation::from_json(
          test::parse_json(
              R"#({
                "input": "Argument(1)",
                "features": ["FeatureOne"]
              })#"),
          context),
      Propagation(
          /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */
          FeatureSet{context.features->get("FeatureOne")}));
  EXPECT_EQ(
      Propagation::from_json(
          test::parse_json(
              R"#({
                "input": "Argument(1)",
                "features": ["FeatureOne", "FeatureTwo"]
              })#"),
          context),
      Propagation(
          /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */
          FeatureSet{
              context.features->get("FeatureOne"),
              context.features->get("FeatureTwo")}));
  EXPECT_EQ(
      Propagation::from_json(
          test::parse_json(
              R"#({
                "input": "Argument(1)",
                "features": ["FeatureOne"],
                "may_features": ["FeatureTwo"]
              })#"),
          context),
      Propagation(
          /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* inferred_features */
          FeatureMayAlwaysSet::make_may({context.features->get("FeatureTwo")}),
          /* user_features */
          FeatureSet{context.features->get("FeatureOne")}));
  EXPECT_EQ(
      Propagation::from_json(
          test::parse_json(
              R"#({
                "input": "Argument(1)",
                "features": ["FeatureOne"],
                "may_features": ["FeatureTwo"],
                "always_features": ["FeatureThree"]
              })#"),
          context),
      Propagation(
          /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* inferred_features */
          FeatureMayAlwaysSet(
              /* may */ FeatureSet{context.features->get("FeatureTwo")},
              /* always */
              FeatureSet{context.features->get("FeatureThree")}),
          /* user_features */ FeatureSet{context.features->get("FeatureOne")}));
  EXPECT_EQ(
      Propagation::from_json(
          test::parse_json(
              R"#({
                "input": "Argument(1)",
                "features": ["FeatureOne"],
                "may_features": [],
                "always_features": []
              })#"),
          context),
      Propagation(
          /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* inferred_features */ FeatureMayAlwaysSet(),
          /* user_features */
          FeatureSet{context.features->get("FeatureOne")}));
}

TEST_F(JsonTest, Model) {
  Scope scope;
  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "LData;",
      /* method_name */ "method",
      /* parameter_types */ "LData;LData;",
      /* return_type*/ "V");

  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* method = context.methods->get(dex_method);

  EXPECT_THROW(
      Model::from_json(method, test::parse_json(R"(1)"), context),
      JsonValidationError);

  EXPECT_EQ(
      Model::from_json(method, test::parse_json(R"({})"), context),
      Model(method, context));
  EXPECT_EQ(Model(method, context).to_json(), test::parse_json(R"({
        "method": "LData;.method:(LData;LData;)V"
      })"));

  EXPECT_THROW(
      Model::from_json(
          method, test::parse_json(R"({"modes": ["invalid-mode"]})"), context),
      JsonValidationError);

  EXPECT_EQ(
      Model::from_json(
          method,
          test::parse_json(R"({
            "modes": [
              "skip-analysis",
              "add-via-obscure-feature",
              "taint-in-taint-out",
              "no-join-virtual-overrides"
            ]
          })"),
          context),
      Model(
          method,
          context,
          Model::Mode::SkipAnalysis | Model::Mode::AddViaObscureFeature |
              Model::Mode::TaintInTaintOut |
              Model::Mode::NoJoinVirtualOverrides));
  EXPECT_EQ(
      test::sorted_json(
          Model(
              method,
              context,
              Model::Mode::SkipAnalysis | Model::Mode::AddViaObscureFeature |
                  Model::Mode::TaintInTaintOut | Model::Mode::TaintInTaintThis |
                  Model::Mode::NoJoinVirtualOverrides)
              .to_json()),
      test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
        "modes": [
          "add-via-obscure-feature",
          "no-join-virtual-overrides",
          "skip-analysis",
          "taint-in-taint-out",
          "taint-in-taint-this"
        ],
        "propagation": [
          {
            "input": "Argument(1)",
            "output": "Argument(0)",
            "always_features": ["via-obscure", "via-obscure-taint-in-taint-this"]
          },
          {
            "input": "Argument(2)",
            "output": "Argument(0)",
            "always_features": ["via-obscure", "via-obscure-taint-in-taint-this"]
          }
        ]
      })#"));

  EXPECT_THROW(
      Model::from_json(
          method, test::parse_json(R"({"generations": {}})"), context),
      JsonValidationError);
  EXPECT_THROW(
      Model::from_json(
          method, test::parse_json(R"({"generations": 1})"), context),
      JsonValidationError);

  EXPECT_EQ(
      Model::from_json(
          method,
          test::parse_json(R"#({
          "generations": [
              {
                "kind": "source_kind",
                "port": "Argument(2)"
              }
            ]
          })#"),
          context),
      Model(
          method,
          context,
          Model::Mode::Normal,
          /* generations */
          {{AccessPath(Root(Root::Kind::Argument, 2)),
            Frame::leaf(context.kinds->get("source_kind"))}}));
  EXPECT_EQ(
      Model(
          method,
          context,
          Model::Mode::Normal,
          /* generations */
          {{AccessPath(Root(Root::Kind::Argument, 2)),
            Frame::leaf(context.kinds->get("source_kind"))}})
          .to_json(),
      test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
        "generations": [
          {
            "kind": "source_kind",
            "caller_port": "Argument(2)",
            "callee_port": "Leaf",
            "origins": ["LData;.method:(LData;LData;)V"]
          }
        ]
      })#"));

  EXPECT_THROW(
      Model::from_json(
          method, test::parse_json(R"({"parameter_sources": {}})"), context),
      JsonValidationError);

  EXPECT_EQ(
      Model::from_json(
          method,
          test::parse_json(R"#({
          "parameter_sources": [
              {
                "kind": "source_kind",
                "port": "Argument(1)"
              }
            ]
          })#"),
          context),
      Model(
          method,
          context,
          Model::Mode::Normal,
          /* generations */ {},
          /* parameter_sources */
          {{AccessPath(Root(Root::Kind::Argument, 1)),
            Frame::leaf(context.kinds->get("source_kind"))}}));
  EXPECT_EQ(
      Model(
          method,
          context,
          Model::Mode::Normal,
          /* generations */ {},
          /* parameter_sources */
          {{AccessPath(Root(Root::Kind::Argument, 1)),
            Frame::leaf(context.kinds->get("source_kind"))}})
          .to_json(),
      test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
        "parameter_sources": [
          {
            "kind": "source_kind",
            "caller_port": "Argument(1)",
            "callee_port": "Leaf",
            "origins": ["LData;.method:(LData;LData;)V"]
          }
        ]
      })#"));

  EXPECT_EQ(
      Model::from_json(
          method,
          test::parse_json(R"#({
          "sources": [
              {
                "kind": "source_kind"
              }
            ]
          })#"),
          context),
      Model(
          method,
          context,
          Model::Mode::Normal,
          /* generations */
          {{AccessPath(Root(Root::Kind::Return)),
            Frame::leaf(context.kinds->get("source_kind"))}},
          /* parameter_sources */ {}));
  EXPECT_EQ(
      Model::from_json(
          method,
          test::parse_json(R"#({
          "sources": [
              {
                "kind": "source_kind",
                "port": "Return"
              }
            ]
          })#"),
          context),
      Model(
          method,
          context,
          Model::Mode::Normal,
          /* generations */
          {{AccessPath(Root(Root::Kind::Return)),
            Frame::leaf(context.kinds->get("source_kind"))}},
          /* parameter_sources */ {}));
  EXPECT_EQ(
      Model::from_json(
          method,
          test::parse_json(R"#({
          "sources": [
              {
                "kind": "source_kind",
                "port": "Argument(1)"
              }
            ]
          })#"),
          context),
      Model(
          method,
          context,
          Model::Mode::Normal,
          /* generations */ {},
          /* parameter_sources */
          {{AccessPath(Root(Root::Kind::Argument, 1)),
            Frame::leaf(context.kinds->get("source_kind"))}}));

  EXPECT_THROW(
      Model::from_json(
          method, test::parse_json(R"({"propagation": 1})"), context),
      JsonValidationError);
  EXPECT_THROW(
      Model::from_json(
          method, test::parse_json(R"({"propagation": {}})"), context),
      JsonValidationError);

  EXPECT_EQ(
      Model::from_json(
          method,
          test::parse_json(R"#({
          "propagation": [
              {
                "input": "Argument(1)",
                "output": "Return"
              },
              {
                "input": "Argument(2)",
                "output": "Argument(0)"
              }
            ]
          })#"),
          context),
      Model(
          method,
          context,
          Model::Mode::Normal,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */
          {
              {Propagation(
                   /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
                   /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                   /* user_features */ FeatureSet::bottom()),
               /* output */ AccessPath(Root(Root::Kind::Return))},
              {Propagation(
                   /* input */ AccessPath(Root(Root::Kind::Argument, 2)),
                   /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                   /* user_features */ FeatureSet::bottom()),
               /* output */ AccessPath(Root(Root::Kind::Argument, 0))},
          }));
  EXPECT_EQ(
      test::sorted_json(
          Model(
              method,
              context,
              Model::Mode::Normal,
              /* generations */ {},
              /* parameter_sources */ {},
              /* sinks */ {},
              /* propagations */
              {
                  {Propagation(
                       /* input */ AccessPath(Root(Root::Kind::Argument, 1)),
                       /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                       /* user_features */ FeatureSet::bottom()),
                   /* output */ AccessPath(Root(Root::Kind::Return))},
                  {Propagation(
                       /* input */ AccessPath(Root(Root::Kind::Argument, 2)),
                       /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                       /* user_features */ FeatureSet::bottom()),
                   /* output */ AccessPath(Root(Root::Kind::Argument, 0))},
              })
              .to_json()),
      test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
        "propagation": [
          {
            "input": "Argument(1)",
            "output": "Return"
          },
          {
            "input": "Argument(2)",
            "output": "Argument(0)"
          }
        ]
      })#"));

  EXPECT_THROW(
      Model::from_json(method, test::parse_json(R"({"sinks": 1})"), context),
      JsonValidationError);
  EXPECT_THROW(
      Model::from_json(method, test::parse_json(R"({"sinks": {}})"), context),
      JsonValidationError);

  EXPECT_EQ(
      Model::from_json(
          method,
          test::parse_json(R"#({
          "sinks": [
              {
                "kind": "first_sink",
                "port": "Argument(2)"
              }
            ]
          })#"),
          context),
      Model(
          method,
          context,
          Model::Mode::Normal,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */
          {
              {AccessPath(Root(Root::Kind::Argument, 2)),
               Frame::leaf(context.kinds->get("first_sink"))},
          }));
  EXPECT_EQ(
      test::sorted_json(Model(
                            method,
                            context,
                            Model::Mode::Normal,
                            /* generations */ {},
                            /* parameter_sources */ {},
                            /* sinks */
                            {
                                {AccessPath(Root(Root::Kind::Argument, 2)),
                                 Frame::leaf(context.kinds->get("first_sink"))},
                            })
                            .to_json()),
      test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
        "sinks": [
              {
                "kind": "first_sink",
                "caller_port": "Argument(2)",
                "callee_port": "Leaf",
                "origins": ["LData;.method:(LData;LData;)V"]
              }
        ]
      })#"));

  EXPECT_EQ(
      Model::from_json(
          method,
          test::parse_json(R"#({
            "attach_to_sources": [
              {
                "port": "Argument(1)",
                "features": ["via-method"]
              }
            ]
          })#"),
          context),
      Model(
          method,
          context,
          Model::Mode::Normal,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* attach_to_sources */
          {{Root(Root::Kind::Argument, 1),
            FeatureSet{context.features->get("via-method")}}}));
  EXPECT_EQ(
      test::sorted_json(Model(
                            method,
                            context,
                            Model::Mode::Normal,
                            /* generations */ {},
                            /* parameter_sources */ {},
                            /* sinks */ {},
                            /* propagations */ {},
                            /* attach_to_sources */
                            {{Root(Root::Kind::Argument, 1),
                              FeatureSet{context.features->get("via-method")}}})
                            .to_json()),
      test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
        "attach_to_sources": [
          {
            "port": "Argument(1)",
            "features": ["via-method"]
          }
        ]
      })#"));

  EXPECT_EQ(
      Model::from_json(
          method,
          test::parse_json(R"#({
            "attach_to_sinks": [
              {
                "port": "Argument(1)",
                "features": ["via-method"]
              }
            ]
          })#"),
          context),
      Model(
          method,
          context,
          Model::Mode::Normal,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* attach_to_sources */ {},
          /* attach_to_sinks */
          {{Root(Root::Kind::Argument, 1),
            FeatureSet{context.features->get("via-method")}}}));
  EXPECT_EQ(
      test::sorted_json(Model(
                            method,
                            context,
                            Model::Mode::Normal,
                            /* generations */ {},
                            /* parameter_sources */ {},
                            /* sinks */ {},
                            /* propagations */ {},
                            /* attach_to_sources */ {},
                            /* attach_to_sinks */
                            {{Root(Root::Kind::Argument, 1),
                              FeatureSet{context.features->get("via-method")}}})
                            .to_json()),
      test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
        "attach_to_sinks": [
          {
            "port": "Argument(1)",
            "features": ["via-method"]
          }
        ]
      })#"));

  EXPECT_EQ(
      Model::from_json(
          method,
          test::parse_json(R"#({
            "attach_to_propagations": [
              {
                "port": "Argument(1)",
                "features": ["via-method"]
              }
            ]
          })#"),
          context),
      Model(
          method,
          context,
          Model::Mode::Normal,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* attach_to_sources */ {},
          /* attach_to_sinks */ {},
          /* attach_to_propagations */
          {{Root(Root::Kind::Argument, 1),
            FeatureSet{context.features->get("via-method")}}}));
  EXPECT_EQ(
      test::sorted_json(Model(
                            method,
                            context,
                            Model::Mode::Normal,
                            /* generations */ {},
                            /* parameter_sources */ {},
                            /* sinks */ {},
                            /* propagations */ {},
                            /* attach_to_sources */ {},
                            /* attach_to_sinks */ {},
                            /* attach_to_propagations */
                            {{Root(Root::Kind::Argument, 1),
                              FeatureSet{context.features->get("via-method")}}})
                            .to_json()),
      test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
        "attach_to_propagations": [
          {
            "port": "Argument(1)",
            "features": ["via-method"]
          }
        ]
      })#"));

  EXPECT_EQ(
      Model::from_json(
          method,
          test::parse_json(R"#({
            "add_features_to_arguments": [
              {
                "port": "Argument(1)",
                "features": ["via-method"]
              }
            ]
          })#"),
          context),
      Model(
          method,
          context,
          Model::Mode::Normal,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* attach_to_sources */ {},
          /* attach_to_sinks */ {},
          /* attach_to_propagations */ {},
          /* add_features_to_arguments */
          {{Root(Root::Kind::Argument, 1),
            FeatureSet{context.features->get("via-method")}}}));
  EXPECT_EQ(
      test::sorted_json(Model(
                            method,
                            context,
                            Model::Mode::Normal,
                            /* generations */ {},
                            /* parameter_sources */ {},
                            /* sinks */ {},
                            /* propagations */ {},
                            /* attach_to_sources */ {},
                            /* attach_to_sinks */ {},
                            /* attach_to_propagations */ {},
                            /* add_features_to_arguments */
                            {{Root(Root::Kind::Argument, 1),
                              FeatureSet{context.features->get("via-method")}}})
                            .to_json()),
      test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
        "attach_to_sources": [
          {
            "port": "Argument(1)",
            "features": ["via-method"]
          }
        ],
        "attach_to_sinks": [
          {
            "port": "Argument(1)",
            "features": ["via-method"]
          }
        ],
        "attach_to_propagations": [
          {
            "port": "Argument(1)",
            "features": ["via-method"]
          }
        ],
        "add_features_to_arguments": [
          {
            "port": "Argument(1)",
            "features": ["via-method"]
          }
        ]
      })#"));

  EXPECT_EQ(
      Model::from_json(
          method,
          test::parse_json(R"#({
            "inline_as": "Argument(1).foo"
          })#"),
          context),
      Model(
          method,
          context,
          Model::Mode::Normal,
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* attach_to_sources */ {},
          /* attach_to_sinks */ {},
          /* attach_to_propagations */ {},
          /* add_features_to_arguments */ {},
          /* inline_as */
          AccessPathConstantDomain(AccessPath(
              Root(Root::Kind::Argument, 1),
              Path{DexString::make_string("foo")}))));
  EXPECT_EQ(
      test::sorted_json(Model(
                            method,
                            context,
                            Model::Mode::Normal,
                            /* generations */ {},
                            /* parameter_sources */ {},
                            /* sinks */ {},
                            /* propagations */ {},
                            /* attach_to_sources */ {},
                            /* attach_to_sinks */ {},
                            /* attach_to_propagations */ {},
                            /* add_features_to_arguments */ {},
                            /* inline_as */
                            AccessPathConstantDomain(AccessPath(
                                Root(Root::Kind::Argument, 1),
                                Path{DexString::make_string("foo")})))
                            .to_json()),
      test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
        "inline_as": "Argument(1).foo"
      })#"));

  // We do not parse issues for now.
  EXPECT_THROW(
      Model::from_json(method, test::parse_json(R"({"issues": 1})"), context),
      JsonValidationError);
  EXPECT_THROW(
      Model::from_json(method, test::parse_json(R"({"issues": {}})"), context),
      JsonValidationError);
  EXPECT_THROW(
      Model::from_json(method, test::parse_json(R"({"issues": []})"), context),
      JsonValidationError);

  auto rule = std::make_unique<SourceSinkRule>(
      /* name */ "Rule",
      /* code */ 1,
      /* description */ "",
      Rule::KindSet{context.kinds->get("first_source")},
      Rule::KindSet{context.kinds->get("first_sink")});
  EXPECT_EQ(
      test::sorted_json(
          Model(
              method,
              context,
              Model::Mode::Normal,
              /* generations */ {},
              /* parameter_sources */ {},
              /* sinks */ {},
              /* propagations */ {},
              /* attach_to_sources */ {},
              /* attach_to_sinks */ {},
              /* attach_to_propagations */ {},
              /* add_features_to_arguments */ {},
              /* inline_as */ AccessPathConstantDomain::bottom(),
              IssueSet{Issue(
                  /* source */ Taint{Frame::leaf(
                      context.kinds->get("first_source"))},
                  /* sink */
                  Taint{Frame::leaf(context.kinds->get("first_sink"))},
                  rule.get(),
                  context.positions->get("Data.java", 1))})
              .to_json()),
      test::parse_json(R"({
        "method": "LData;.method:(LData;LData;)V",
        "issues": [
          {
            "rule": 1,
            "position": {
              "path": "Data.java",
              "line": 1
            },
            "sources": [
              {
                "kind": "first_source",
                "callee_port": "Leaf"
              }
            ],
            "sinks": [
              {
                "kind": "first_sink",
                "callee_port": "Leaf"
              }
            ]
          }
        ]
      })"));
}

TEST_F(JsonTest, LifecycleMethod) {
  EXPECT_THROW(
      LifecycleMethod::from_json(test::parse_json("{}")), JsonValidationError);
  EXPECT_THROW(
      LifecycleMethod::from_json(test::parse_json("1")), JsonValidationError);
  EXPECT_THROW(
      LifecycleMethod::from_json(test::parse_json(R"({
        "base_class_name": "Landroidx/fragment/app/FragmentActivity;",
        "method_name": "activity_lifecycle_wrapper",
        "callees": []
      })")),
      JsonValidationError);

  EXPECT_EQ(
      LifecycleMethod::from_json(test::parse_json(R"({
        "base_class_name": "Landroidx/fragment/app/FragmentActivity;",
        "method_name": "activity_lifecycle_wrapper",
        "callees": [
          {
            "method_name": "onCreate",
            "return_type": "V",
            "argument_types": [
              "Landroid/os/Bundle;"
            ]
          },
          {
            "method_name": "onStart",
            "return_type": "V",
            "argument_types": []
          }
        ]
      })")),
      LifecycleMethod(
          /* base_class_name */ "Landroidx/fragment/app/FragmentActivity;",
          /* method_name */ "activity_lifecycle_wrapper",
          /* callees */
          {LifecycleMethodCall("onCreate", "V", {"Landroid/os/Bundle;"}),
           LifecycleMethodCall("onStart", "V", {})}));
}

} // namespace marianatrench

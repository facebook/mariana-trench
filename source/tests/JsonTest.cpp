/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/CanonicalName.h>
#include <mariana-trench/Constants.h>
#include <mariana-trench/Field.h>
#include <mariana-trench/FieldModel.h>
#include <mariana-trench/Fields.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/LifecycleMethod.h>
#include <mariana-trench/LifecycleMethods.h>
#include <mariana-trench/LocalPositionSet.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/MultiSourceMultiSinkRule.h>
#include <mariana-trench/PartialKind.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/Sanitizer.h>
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

TEST_F(JsonTest, Root) {
  EXPECT_THROW(Root::from_json(test::parse_json("{}")), JsonValidationError);
  EXPECT_THROW(Root::from_json(test::parse_json("1")), JsonValidationError);
  EXPECT_THROW(
      Root::from_json(test::parse_json("\"Foo\"")), JsonValidationError);
  EXPECT_JSON_EQ(Root, "\"Return\"", Root(Root::Kind::Return));
  EXPECT_JSON_EQ(Root, "\"Leaf\"", (Root(Root::Kind::Leaf)));
  EXPECT_JSON_EQ(Root, "\"Anchor\"", Root(Root::Kind::Anchor));
  EXPECT_JSON_EQ(Root, "\"Producer\"", Root(Root::Kind::Producer));
  EXPECT_JSON_EQ(Root, "\"call-chain\"", Root(Root::Kind::CallEffectCallChain));
  EXPECT_JSON_EQ(Root, "\"Argument(0)\"", Root(Root::Kind::Argument, 0));
  EXPECT_JSON_EQ(Root, "\"Argument(1)\"", Root(Root::Kind::Argument, 1));
  EXPECT_JSON_EQ(Root, "\"Argument(12)\"", Root(Root::Kind::Argument, 12));
  EXPECT_THROW(
      Root::from_json(test::parse_json("\"Argument(0\"")), JsonValidationError);
  EXPECT_THROW(
      Root::from_json(test::parse_json("\"Argument()\"")), JsonValidationError);
  EXPECT_THROW(
      Root::from_json(test::parse_json("\"Argument(x)\"")),
      JsonValidationError);
  EXPECT_THROW(
      Root::from_json(test::parse_json("\"Argument(x0)\"")),
      JsonValidationError);
  EXPECT_THROW(
      Root::from_json(test::parse_json("\"Argument(0x)\"")),
      JsonValidationError);
  EXPECT_THROW(
      Root::from_json(test::parse_json("\"Argument(-1)\"")),
      JsonValidationError);
}

TEST_F(JsonTest, PathElementTest) {
  const auto field = PathElement::field("field");
  const auto invalid_field = PathElement::field("<field>");

  const auto index = PathElement::index("index");
  const auto index_with_dot = PathElement::index("a.b");

  const auto value_of_argument_1 =
      PathElement::index_from_value_of(Root(Root::Kind::Argument, 1));

  const auto any_index = PathElement::any_index();

  EXPECT_EQ(PathElement::from_json("field"), field);
  EXPECT_EQ(PathElement::from_json("[index]"), index);
  EXPECT_EQ(PathElement::from_json("[*]"), any_index);
  EXPECT_EQ(PathElement::from_json("[<Argument(1)>]"), value_of_argument_1);

  // index_from_value_of syntax is `[<` ... `>]`.
  EXPECT_EQ(
      AccessPath::from_json("Argument(0)[<Argument(1)>]"),
      AccessPath(Root(Root::Kind::Argument, 0), Path{value_of_argument_1}));

  // Angle braces `<`...`>` within a field does not have a special meaning.
  EXPECT_EQ(
      AccessPath::from_json("Argument(0).<field>"),
      AccessPath(Root(Root::Kind::Argument, 0), Path{invalid_field}));

  // Anything enclosed within `[]` is an index.
  EXPECT_EQ(
      AccessPath::from_json("Argument(0)[a.b]"),
      AccessPath(Root(Root::Kind::Argument, 0), Path{index_with_dot}));

  // All path elements together
  auto access_path1 =
      AccessPath::from_json("Argument(0).field[index][*].field[<Argument(1)>]");
  EXPECT_EQ(access_path1.path().size(), 5);
  EXPECT_EQ(
      access_path1,
      AccessPath(
          Root(Root::Kind::Argument, 0),
          Path{field, index, any_index, field, value_of_argument_1}));

  // Angle braces `<`...`>` on its own is invalid.
  EXPECT_THROW(
      AccessPath::from_json("Argument(0)<field>"), JsonValidationError);

  // Empty fields not allowed
  EXPECT_THROW(AccessPath::from_json("Argument(0)..."), JsonValidationError);
  EXPECT_THROW(
      AccessPath::from_json("Argument(0)...field"), JsonValidationError);

  // Empty index not allowed.
  EXPECT_THROW(AccessPath::from_json("Argument(0)[]"), JsonValidationError);
  EXPECT_THROW(AccessPath::from_json("Argument(0).[]"), JsonValidationError);

  // Only allow Root::Kind::Argument
  EXPECT_THROW(
      AccessPath::from_json("Argument(0).field[index][*][<Return>]"),
      JsonValidationError);

  // Invalid syntax for PathElement
  EXPECT_THROW(AccessPath::from_json("Argument(0)]"), JsonValidationError);

  // Invalid syntax for PathElement
  EXPECT_THROW(AccessPath::from_json("Argument(0)]["), JsonValidationError);
}

TEST_F(JsonTest, AccessPath) {
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  EXPECT_JSON_EQ(
      AccessPath, "\"Return\"", AccessPath(Root(Root::Kind::Return)));
  EXPECT_JSON_EQ(AccessPath, "\"Leaf\"", AccessPath(Root(Root::Kind::Leaf)));
  EXPECT_JSON_EQ(
      AccessPath, "\"Argument(0)\"", AccessPath(Root(Root::Kind::Argument, 0)));
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

TEST_F(JsonTest, Field) {
  Scope scope;
  const auto* dex_field = redex::create_field(
      scope,
      /* class_name */ "LBase;",
      /* fields */
      {"field1", type::java_lang_String()});

  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  const auto* field = context.fields->get(dex_field);

  EXPECT_THROW(
      Field::from_json(test::parse_json(R"({})"), context),
      JsonValidationError);
  EXPECT_THROW(
      Field::from_json(
          test::parse_json(R"("LBase;.non_existing:Ljava/lang/String;")"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      Field::from_json(
          test::parse_json(R"("LBase;.field1:Ljava/lang/Object;")"), context),
      JsonValidationError);
  EXPECT_JSON_EQ(
      Field, R"("LBase;.field1:Ljava/lang/String;")", field, context);
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

TEST_F(JsonTest, Sanitizer) {
  Scope scope;
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  EXPECT_THROW(
      Sanitizer::from_json(
          test::parse_json(R"({"sanitize": "Sources"})"), context),
      JsonValidationError);
  EXPECT_THROW(
      Sanitizer::from_json(
          test::parse_json(R"({"Sanitize": "sinks"})"), context),
      JsonValidationError);
  // kinds cannot be an empty array
  EXPECT_THROW(
      Sanitizer::from_json(
          test::parse_json(R"({"sanitize": "sinks", "kinds": []})"), context),
      JsonValidationError);
  // propagation sanitizer can't have kinds
  EXPECT_THROW(
      Sanitizer::from_json(
          test::parse_json(
              R"({"sanitize": "propgation", "kinds": [{"kind": "Kind1"}]})"),
          context),
      JsonValidationError);

  EXPECT_JSON_EQ(
      Sanitizer,
      R"({"sanitize": "sources"})",
      Sanitizer(
          SanitizerKind::Sources,
          /* kinds */ KindSetAbstractDomain::top()),
      context);
  const auto* kind1 = context.kind_factory->get("Kind1");
  const auto* kind2 = context.kind_factory->get("Kind2");
  const auto* kind3 = context.kind_factory->get("Kind3");
  const auto* partial_kind = context.kind_factory->get_partial("Kind3", "a");
  const auto* partial_kind2 = context.kind_factory->get_partial("Kind3", "b");
  // Test from_json
  EXPECT_EQ(
      Sanitizer::from_json(
          test::parse_json(
              R"({"sanitize": "sources", "kinds": [{"kind": "Kind1"}, {"kind": "Kind2"}]})"),
          context),
      Sanitizer(
          SanitizerKind::Sources,
          /* kinds */ KindSetAbstractDomain({kind1, kind2})));
  EXPECT_EQ(
      Sanitizer::from_json(
          test::parse_json(
              R"({"sanitize": "sinks", "kinds": [{"kind": "Kind1"}, {"kind": "Kind3", "partial_label": "a"}, {"kind": "Kind3", "partial_label": "b"}]})"),
          context),
      Sanitizer(
          SanitizerKind::Sinks,
          /* kinds */
          KindSetAbstractDomain({kind1, partial_kind, partial_kind2})));

  // Test to_json
  EXPECT_EQ(
      test::parse_json(
          R"({"sanitize": "sources", "kinds": [{"kind": "Kind1"}, {"kind": "Kind2"}]})"),
      test::sorted_json(Sanitizer(
                            SanitizerKind::Sources,
                            /* kinds */ KindSetAbstractDomain({kind1, kind2}))
                            .to_json()));
  EXPECT_EQ(
      test::parse_json(
          R"({"sanitize": "sources", "kinds": [{"kind": "Kind1"}, {"kind": "Kind2"}, {"kind": "Partial:Kind3:a"}]})"),
      test::sorted_json(
          Sanitizer(
              SanitizerKind::Sources,
              /* kinds */ KindSetAbstractDomain({kind1, kind2, partial_kind}))
              .to_json()));
  EXPECT_EQ(
      test::parse_json(
          R"({"sanitize": "sinks", "kinds": [{"kind": "Kind1"}, {"kind": "Kind3"}]})"),
      test::sorted_json(Sanitizer(
                            SanitizerKind::Sinks,
                            /* kinds */ KindSetAbstractDomain({kind1, kind3}))
                            .to_json()));
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
      testing::ElementsAre(context.kind_factory->get("rule_source")));
  EXPECT_THAT(
      rule_with_single_source_and_sink->sink_kinds(),
      testing::ElementsAre(context.kind_factory->get("rule_sink")));

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
          context.kind_factory->get("rule_source_one"),
          context.kind_factory->get("rule_source_two")));
  EXPECT_THAT(
      rule_with_multiple_sources_and_sinks->sink_kinds(),
      testing::UnorderedElementsAre(
          context.kind_factory->get("rule_sink_one"),
          context.kind_factory->get("rule_sink_two")));

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
          context.kind_factory->get_partial("rule_sink", "labelA")));
  EXPECT_THAT(
      rule_with_combined_sources->partial_sink_kinds("labelB"),
      testing::UnorderedElementsAre(
          context.kind_factory->get_partial("rule_sink", "labelB")));
  EXPECT_TRUE(rule_with_combined_sources->partial_sink_kinds("labelC").empty());
  const auto& multi_sources = rule_with_combined_sources->multi_source_kinds();
  EXPECT_THAT(
      multi_sources.find("labelA")->second,
      testing::UnorderedElementsAre(
          context.kind_factory->get("rule_source_one"),
          context.kind_factory->get("rule_source_two")));
  EXPECT_THAT(
      multi_sources.find("labelB")->second,
      testing::UnorderedElementsAre(
          context.kind_factory->get("rule_source_one")));
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

TEST_F(JsonTest, TaintConfig) {
  Scope scope;
  auto* dex_source_one =
      redex::create_void_method(scope, "LClassOne;", "source");

  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* source_one = context.methods->get(dex_source_one);

  EXPECT_THROW(
      TaintConfig::from_json(test::parse_json(R"(1)"), context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(test::parse_json(R"({})"), context),
      JsonValidationError);

  // Parse the kind.
  EXPECT_THROW(
      TaintConfig::from_json(test::parse_json(R"({"kind": 1})"), context),
      JsonValidationError);
  EXPECT_EQ(
      TaintConfig::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource"
              })"),
          context),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .inferred_features = FeatureMayAlwaysSet::bottom(),
              .locally_inferred_features = FeatureMayAlwaysSet::bottom()}));

  // Parse the kind for partial leaves.
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(R"({"kind": "TestSink", "partial_label": 1})"),
          context),
      JsonValidationError);
  auto frame = TaintConfig::from_json(
      test::parse_json(
          R"({
                "kind": "TestSink",
                "partial_label": "X"
              })"),
      context);
  EXPECT_EQ(
      frame,
      test::make_taint_config(
          /* kind */ context.kind_factory->get_partial("TestSink", "X"),
          test::FrameProperties{
              .inferred_features = FeatureMayAlwaysSet::bottom(),
              .locally_inferred_features = FeatureMayAlwaysSet::bottom()}));
  const auto* frame_kind = frame.kind()->as<PartialKind>();
  EXPECT_NE(frame_kind, nullptr);
  EXPECT_EQ(frame_kind->name(), "TestSink");
  EXPECT_EQ(frame_kind->label(), "X");
  EXPECT_EQ(frame_kind->as<TriggeredPartialKind>(), nullptr);

  // Parse the callee port.
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "callee_port": "InvalidPort"
          })"),
          context),
      JsonValidationError);

  // Parse the callee, position and distance.
  EXPECT_EQ(
      TaintConfig::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "callee_port": "Leaf"
          })"),
          context),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .inferred_features = FeatureMayAlwaysSet::bottom(),
              .locally_inferred_features = FeatureMayAlwaysSet::bottom()}));
  EXPECT_EQ(
      TaintConfig::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "callee_port": "Return",
            "callee": "LClassOne;.source:()V",
            "call_position": {},
            "distance": 1
          })"),
          context),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = source_one,
              .call_position = context.positions->unknown(),
              .distance = 1,
              .inferred_features = FeatureMayAlwaysSet::bottom(),
              .locally_inferred_features = FeatureMayAlwaysSet::bottom()}));
  EXPECT_EQ(
      TaintConfig::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "callee_port": "Return",
            "callee": "LClassOne;.source:()V",
            "call_position": {"line": 2, "path": "Object.java"},
            "distance": 2
          })"),
          context),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = source_one,
              .call_position = context.positions->get("Object.java", 2),
              .distance = 2,
              .inferred_features = FeatureMayAlwaysSet::bottom(),
              .locally_inferred_features = FeatureMayAlwaysSet::bottom()}));

  // Parse the features.
  EXPECT_EQ(
      TaintConfig::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "callee_port": "Leaf",
            "always_features": ["FeatureOne"]
          })"),
          context),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .inferred_features =
                  FeatureMayAlwaysSet{
                      context.feature_factory->get("FeatureOne")},
              .locally_inferred_features = FeatureMayAlwaysSet::bottom()}));
  EXPECT_EQ(
      TaintConfig::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "callee_port": "Leaf",
            "always_features": ["FeatureOne", "FeatureTwo"]
          })"),
          context),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .inferred_features =
                  FeatureMayAlwaysSet{
                      context.feature_factory->get("FeatureOne"),
                      context.feature_factory->get("FeatureTwo")},
              .locally_inferred_features = FeatureMayAlwaysSet::bottom()}));
  EXPECT_EQ(
      TaintConfig::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "callee_port": "Leaf",
            "may_features": ["FeatureOne"],
            "always_features": ["FeatureTwo"]
          })"),
          context),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .inferred_features = FeatureMayAlwaysSet(
                  /* may */ FeatureSet{context.feature_factory->get(
                      "FeatureOne")},
                  /* always */
                  FeatureSet{context.feature_factory->get("FeatureTwo")}),
              .locally_inferred_features = FeatureMayAlwaysSet::bottom()}));
  EXPECT_EQ(
      TaintConfig::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "callee_port": "Leaf",
            "may_features": ["FeatureOne"]
          })"),
          context),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .inferred_features = FeatureMayAlwaysSet(
                  /* may */ FeatureSet{context.feature_factory->get(
                      "FeatureOne")},
                  /* always */ FeatureSet{}),
              .locally_inferred_features = FeatureMayAlwaysSet::bottom()}));
  EXPECT_EQ(
      TaintConfig::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "callee_port": "Leaf",
            "may_features": ["FeatureOne", "FeatureTwo"]
          })"),
          context),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .inferred_features = FeatureMayAlwaysSet(
                  /* may */
                  FeatureSet{
                      context.feature_factory->get("FeatureOne"),
                      context.feature_factory->get("FeatureTwo")},
                  /* always */ FeatureSet{}),
              .locally_inferred_features = FeatureMayAlwaysSet::bottom()}));
  EXPECT_EQ(
      TaintConfig::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "features": ["FeatureOne"]
              })"),
          context),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .inferred_features = FeatureMayAlwaysSet::bottom(),
              .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
              .user_features =
                  FeatureSet{context.feature_factory->get("FeatureOne")}}));
  EXPECT_EQ(
      TaintConfig::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "features": ["FeatureOne", "FeatureTwo"]
              })"),
          context),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .inferred_features = FeatureMayAlwaysSet::bottom(),
              .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
              .user_features = FeatureSet{
                  context.feature_factory->get("FeatureOne"),
                  context.feature_factory->get("FeatureTwo")}}));
  EXPECT_EQ(
      TaintConfig::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "features": ["FeatureOne"],
                "may_features": ["FeatureTwo"]
              })"),
          context),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .inferred_features = FeatureMayAlwaysSet(
                  /* may */ FeatureSet{context.feature_factory->get(
                      "FeatureTwo")},
                  /* always */ FeatureSet{}),
              .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
              .user_features =
                  FeatureSet{context.feature_factory->get("FeatureOne")}}));
  EXPECT_EQ(
      TaintConfig::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "features": ["FeatureOne"],
                "may_features": ["FeatureTwo"],
                "always_features": ["FeatureThree"]
              })"),
          context),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .inferred_features = FeatureMayAlwaysSet(
                  /* may */ FeatureSet{context.feature_factory->get(
                      "FeatureTwo")},
                  /* always */
                  FeatureSet{context.feature_factory->get("FeatureThree")}),
              .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
              .user_features =
                  FeatureSet{context.feature_factory->get("FeatureOne")}}));
  EXPECT_EQ(
      TaintConfig::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "features": ["FeatureOne"],
                "may_features": [],
                "always_features": []
              })"),
          context),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
              .user_features =
                  FeatureSet{context.feature_factory->get("FeatureOne")}}));

  // Parse via_type_of_ports
  EXPECT_EQ(
      TaintConfig::from_json(
          test::parse_json(R"#({
            "kind": "TestSource",
            "callee_port": "Leaf",
            "via_type_of": ["Argument(1)", "Return"]
          })#"),
          context),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .inferred_features = FeatureMayAlwaysSet::bottom(),
              .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
              .via_type_of_ports = RootSetAbstractDomain(
                  {Root(Root::Kind::Return), Root(Root::Kind::Argument, 1)})}));

  // Consistency checks.
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "callee_port": "Return"
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "call_position": {}
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "distance": 1
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "callee_port": "Leaf",
            "callee": "LClassOne;.source:()V"
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(R"({
            "kind": "TestSource",
            "callee_port": "Return",
            "callee": "LClassOne;.source:()V"
          })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(
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
      TaintConfig::from_json(test::parse_json(R"(1)"), context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(test::parse_json(R"({})"), context),
      JsonValidationError);

  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(R"({"kind": "TestSource", "canonical_names": []})"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(
              R"({"kind": "TestSource", "callee_port": "Anchor", "canonical_names": []})"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(
              R"({"kind": "TestSource", "callee_port": "Producer"})"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(
              R"({"kind": "TestSource", "canonical_names": [ { "irrelevant": "field" } ]})"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "canonical_names": [ { "template": "%programmatic_leaf_name%", "instantiated": "MyMethod::MyClass" } ]
              })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(
              R"#({
                "kind": "TestSource",
                "callee_port": "Argument(0)",
                "canonical_names": [ { "template": "%programmatic_leaf_name%" } ]
              })#"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "callee_port": "Producer",
                "canonical_names": [ { "template": "%programmatic_leaf_name%" } ]
              })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "callee_port": "Anchor",
                "canonical_names": [ { "instantiated": "MyMethod::MyClass" } ]
              })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "canonical_names": [
                  { "instantiated": "MyMethod::MyClass" },
                  { "template": "%programmatic_leaf_name%" }
                ]
              })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "canonical_names": [
                  { "template": "%via_type_of%" }
                ]
              })"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(
              R"#({
                "kind": "TestSource",
                "via_type_of": [ "Argument(1)", "Return" ],
                "canonical_names": [
                  { "template": "%via_type_of%" }
                ]
              })#"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      TaintConfig::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "canonical_names": [ {"instantiated": "Lcom/android/MyClass;.MyMethod"} ]
              })"),
          context),
      JsonValidationError);
  EXPECT_EQ(
      TaintConfig::from_json(
          test::parse_json(
              R"({
                "kind": "TestSource",
                "canonical_names": [ {"template": "%programmatic_leaf_name%"} ]
              })"),
          context),
      TaintConfig(
          context.kind_factory->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Anchor)),
          /* callee */ nullptr,
          /* call_kind */ CallKind::declaration(),
          /* call_position */ nullptr,
          /* type_contexts */ CallClassIntervalContext(),
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* via_value_of_ports */ {},
          /* canonical_names */
          CanonicalNameSetAbstractDomain{CanonicalName(
              CanonicalName::TemplateValue{"%programmatic_leaf_name%"})},
          /* output_paths */ {},
          /* local_positions */ {},
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* extra_traces */ {}));

  // CRTEX consumer frames (identified by "producer" port) are a special case.
  // They are "origins" rather than declarations, and the origins should contain
  // information leading to the next hop (in the CRTEX producer run), i.e. the
  // canonical port and name for the leaf.
  auto producer_port = AccessPath(
      Root(Root::Kind::Producer),
      Path{PathElement::field("123"), PathElement::field("formal(0)")});
  auto expected_taint_config = TaintConfig(
      context.kind_factory->get("TestSource"),
      /* callee_port */ producer_port,
      /* callee */ nullptr,
      /* call_kind */ CallKind::origin(),
      /* call_position */ nullptr,
      /* type_contexts */ CallClassIntervalContext(),
      /* distance */ 0,
      /* origins */
      OriginSet{context.origin_factory->crtex_origin(
          /* canonical_name */ "Lcom/android/MyClass;.MyMethod",
          /* port */ context.access_path_factory->get(producer_port))},
      /* inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* user_features */ {},
      /* via_type_of_ports */ {},
      /* via_value_of_ports */ {},
      /* canonical_names */
      CanonicalNameSetAbstractDomain{CanonicalName(
          CanonicalName::InstantiatedValue{"Lcom/android/MyClass;.MyMethod"})},
      /* output_paths */ {},
      /* local_positions */ {},
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* extra_traces */ {});
  EXPECT_EQ(
      TaintConfig::from_json(
          test::parse_json(
              R"#({
                "kind": "TestSource",
                "callee_port": "Producer.123.formal(0)",
                "canonical_names": [ {"instantiated": "Lcom/android/MyClass;.MyMethod"} ]
              })#"),
          context),
      expected_taint_config);
}

TEST_F(JsonTest, Frame) {
  Scope scope;
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  EXPECT_EQ(
      test::sorted_json(
          test::make_taint_frame(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{
                  .inferred_features = FeatureMayAlwaysSet::make_always(
                      {context.feature_factory->get("FeatureTwo")}),
                  .user_features = FeatureSet(
                      {context.feature_factory->get("FeatureThree")})})
              .to_json(CallInfo::make_default(), ExportOriginsMode::Always)),
      test::sorted_json(test::parse_json(R"({
          "kind": "TestSource",
          "always_features": ["FeatureTwo", "FeatureThree"]
        })")));

  EXPECT_EQ(
      test::sorted_json(
          test::make_taint_frame(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{
                  .class_interval_context = CallClassIntervalContext(
                      ClassIntervals::Interval::finite(1, 2),
                      /* preserves_type_context */ true)})
              .to_json(CallInfo::make_default(), ExportOriginsMode::Always)),
      test::sorted_json(test::parse_json(R"({
          "callee_interval": [1, 2],
          "kind": "TestSource",
          "preserves_type_context": true
        })")));
}

TEST_F(JsonTest, Propagation) {
  auto context = test::make_empty_context();
  auto empty_input_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}};

  EXPECT_THROW(
      PropagationConfig::from_json(test::parse_json(R"({})"), context),
      JsonValidationError);
  EXPECT_THROW(
      PropagationConfig::from_json(
          test::parse_json(R"#({"input": 1, "output": "Return"})#"), context),
      JsonValidationError);
  EXPECT_THROW(
      PropagationConfig::from_json(
          test::parse_json(R"#({"input": "x", "output": "Return"})#"), context),
      JsonValidationError);
  EXPECT_THROW(
      PropagationConfig::from_json(
          test::parse_json(R"#({"input": "Argument(0)", "output": 1})#"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      PropagationConfig::from_json(
          test::parse_json(R"#({"input": "Argument(0)", "output": "x"})#"),
          context),
      JsonValidationError);

  EXPECT_EQ(
      PropagationConfig::from_json(
          test::parse_json(R"#({"input": "Argument(1)", "output": "Return"})#"),
          context),
      PropagationConfig(
          /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* kind */ context.kind_factory->local_return(),
          /* output_paths */
          PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {}));

  EXPECT_EQ(
      PropagationConfig::from_json(
          test::parse_json(
              R"#({"input": "Argument(1).x", "output": "Return", "always_features": ["FeatureOne"]})#"),
          context),
      PropagationConfig(
          /* input_path */ AccessPath(
              Root(Root::Kind::Argument, 1), Path{PathElement::field("x")}),
          /* kind */ context.kind_factory->local_return(),
          /* output_paths */
          PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          /* inferred_features */
          FeatureMayAlwaysSet{context.feature_factory->get("FeatureOne")},
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {}));

  EXPECT_EQ(
      PropagationConfig::from_json(
          test::parse_json(
              R"#({"input": "Argument(0)", "output": "Argument(1).x", "always_features": ["FeatureOne"]})#"),
          context),
      PropagationConfig(
          /* input_path */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* kind */ context.kind_factory->local_argument(1),
          /* output_paths */
          PathTreeDomain{
              {Path{PathElement::field("x")}, CollapseDepth::zero()}},
          /* inferred_features */
          FeatureMayAlwaysSet{context.feature_factory->get("FeatureOne")},
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {}));

  EXPECT_EQ(
      PropagationConfig::from_json(
          test::parse_json(R"#({
        "input": "Argument(1)",
        "output": "Return",
        "may_features": ["FeatureOne"],
        "always_features": ["FeatureTwo"]
      })#"),
          context),
      PropagationConfig(
          /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* kind */ context.kind_factory->local_return(),
          /* output_paths */
          PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          /* inferred_features */
          FeatureMayAlwaysSet(
              /* may */ FeatureSet{context.feature_factory->get("FeatureOne")},
              /* always */
              FeatureSet{context.feature_factory->get("FeatureTwo")}),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {}));
  EXPECT_EQ(
      PropagationConfig::from_json(
          test::parse_json(R"#({
        "input": "Argument(1)",
        "output": "Return",
        "may_features": ["FeatureOne"],
      })#"),
          context),
      PropagationConfig(
          /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* kind */ context.kind_factory->local_return(),
          /* output_paths */
          PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          /* inferred_features */
          FeatureMayAlwaysSet(
              /* may */ FeatureSet{context.feature_factory->get("FeatureOne")},
              /* always */ FeatureSet{}),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {}));
  EXPECT_EQ(
      PropagationConfig::from_json(
          test::parse_json(
              R"#({
                "input": "Argument(1)",
                "output": "Return",
                "features": ["FeatureOne"]
              })#"),
          context),
      PropagationConfig(
          /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* kind */ context.kind_factory->local_return(),
          /* output_paths */
          PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */
          FeatureSet{context.feature_factory->get("FeatureOne")}));
  EXPECT_EQ(
      PropagationConfig::from_json(
          test::parse_json(
              R"#({
                "input": "Argument(1)",
                "output": "Return",
                "features": ["FeatureOne", "FeatureTwo"]
              })#"),
          context),
      PropagationConfig(
          /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* kind */ context.kind_factory->local_return(),
          /* output_paths */
          PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */
          FeatureSet{
              context.feature_factory->get("FeatureOne"),
              context.feature_factory->get("FeatureTwo")}));
  EXPECT_EQ(
      PropagationConfig::from_json(
          test::parse_json(
              R"#({
                "input": "Argument(1)",
                "output": "Return",
                "features": ["FeatureOne"],
                "may_features": ["FeatureTwo"]
              })#"),
          context),
      PropagationConfig(
          /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* kind */ context.kind_factory->local_return(),
          /* output_paths */
          PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          /* inferred_features */
          FeatureMayAlwaysSet::make_may(
              {context.feature_factory->get("FeatureTwo")}),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */
          FeatureSet{context.feature_factory->get("FeatureOne")}));
  EXPECT_EQ(
      PropagationConfig::from_json(
          test::parse_json(
              R"#({
                "input": "Argument(1)",
                "output": "Return",
                "features": ["FeatureOne"],
                "may_features": ["FeatureTwo"],
                "always_features": ["FeatureThree"]
              })#"),
          context),
      PropagationConfig(
          /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* kind */ context.kind_factory->local_return(),
          /* output_paths */
          PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          /* inferred_features */
          FeatureMayAlwaysSet(
              /* may */ FeatureSet{context.feature_factory->get("FeatureTwo")},
              /* always */
              FeatureSet{context.feature_factory->get("FeatureThree")}),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */
          FeatureSet{context.feature_factory->get("FeatureOne")}));
  EXPECT_EQ(
      PropagationConfig::from_json(
          test::parse_json(
              R"#({
                "input": "Argument(1)",
                "output": "Return",
                "features": ["FeatureOne"],
                "may_features": [],
                "always_features": []
              })#"),
          context),
      PropagationConfig(
          /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* kind */ context.kind_factory->local_return(),
          /* output_paths */
          PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          /* inferred_features */ FeatureMayAlwaysSet(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */
          FeatureSet{context.feature_factory->get("FeatureOne")}));

  EXPECT_THROW(
      PropagationConfig::from_json(
          test::parse_json(
              R"#({
                "input": "Argument(0)",
                "output": "Return",
                "collapse": 42,
              })#"),
          context),
      JsonValidationError);
  EXPECT_EQ(
      PropagationConfig::from_json(
          test::parse_json(
              R"#({
                "input": "Argument(1)",
                "output": "Return",
                "collapse": true,
              })#"),
          context),
      PropagationConfig(
          /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* kind */ context.kind_factory->local_return(),
          /* output_paths */
          PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {}));
  EXPECT_EQ(
      PropagationConfig::from_json(
          test::parse_json(
              R"#({
                "input": "Argument(1)",
                "output": "Return",
                "collapse": false,
              })#"),
          context),
      PropagationConfig(
          /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* kind */ context.kind_factory->local_return(),
          /* output_paths */
          PathTreeDomain{{Path{}, CollapseDepth::no_collapse()}},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {}));

  EXPECT_THROW(
      PropagationConfig::from_json(
          test::parse_json(
              R"#({
                "input": "Argument(0)",
                "output": "Return",
                "collapse-depth": -1,
              })#"),
          context),
      JsonValidationError);
  EXPECT_EQ(
      PropagationConfig::from_json(
          test::parse_json(
              R"#({
                "input": "Argument(1)",
                "output": "Return",
                "collapse-depth": 0,
              })#"),
          context),
      PropagationConfig(
          /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* kind */ context.kind_factory->local_return(),
          /* output_paths */
          PathTreeDomain{{Path{}, CollapseDepth(0)}},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {}));
  EXPECT_EQ(
      PropagationConfig::from_json(
          test::parse_json(
              R"#({
                "input": "Argument(1)",
                "output": "Return",
                "collapse-depth": 10,
              })#"),
          context),
      PropagationConfig(
          /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* kind */ context.kind_factory->local_return(),
          /* output_paths */
          PathTreeDomain{{Path{}, CollapseDepth(10)}},
          /* inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ {}));
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
  EXPECT_EQ(
      Model(method, context).to_json(ExportOriginsMode::Always),
      test::parse_json(R"({
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
              "no-join-virtual-overrides",
              "no-collapse-on-propagation",
              "alias-memory-location-on-invoke"
            ]
          })"),
          context),
      Model(
          method,
          context,
          Model::Mode::SkipAnalysis | Model::Mode::AddViaObscureFeature |
              Model::Mode::TaintInTaintOut |
              Model::Mode::NoJoinVirtualOverrides |
              Model::Mode::NoCollapseOnPropagation |
              Model::Mode::AliasMemoryLocationOnInvoke));
  EXPECT_EQ(
      test::sorted_json(
          Model(
              method,
              context,
              Model::Mode::SkipAnalysis | Model::Mode::AddViaObscureFeature |
                  Model::Mode::TaintInTaintOut | Model::Mode::TaintInTaintThis |
                  Model::Mode::NoJoinVirtualOverrides |
                  Model::Mode::NoCollapseOnPropagation |
                  Model::Mode::AliasMemoryLocationOnInvoke)
              .to_json(ExportOriginsMode::Always)),
      test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
        "modes": [
          "add-via-obscure-feature",
          "alias-memory-location-on-invoke",
          "no-collapse-on-propagation",
          "no-join-virtual-overrides",
          "skip-analysis",
          "taint-in-taint-out",
          "taint-in-taint-this"
        ],
        "propagation": [
          {
            "input": "Argument(1)",
            "output":
            [
              {
                "call_info": {
                  "call_kind": "Propagation",
                  "port": "Argument(0)"
                },
                "kinds": [
                  {
                    "always_features": [ "via-obscure" ],
                    "kind": "LocalArgument(0)",
                    "output_paths": { "": 0 }
                  }
                ]
              }
            ]
          },
          {
            "input": "Argument(2)",
            "output":
            [
              {
                "call_info": {
                  "call_kind": "Propagation",
                  "port": "Argument(0)"
                },
                "kinds": [
                  {
                    "always_features": [ "via-obscure" ],
                    "kind": "LocalArgument(0)",
                    "output_paths": { "": 0 }
                  }
                ]
              }
            ]
          }
        ]
      })#"));

  EXPECT_THROW(
      Model::from_json(
          method,
          test::parse_json(R"({"freeze": ["invalid-freeze-kind"]})"),
          context),
      JsonValidationError);

  EXPECT_EQ(
      Model::from_json(
          method,
          test::parse_json(R"({
            "freeze": [
              "generations",
              "parameter_sources",
              "sinks",
              "propagation"
            ]
          })"),
          context),
      Model(
          method,
          context,
          {},
          Model::FreezeKind::Generations | Model::FreezeKind::ParameterSources |
              Model::FreezeKind::Sinks | Model::FreezeKind::Propagations));
  EXPECT_EQ(
      test::sorted_json(Model(
                            method,
                            context,
                            {},
                            Model::FreezeKind::Generations |
                                Model::FreezeKind::ParameterSources |
                                Model::FreezeKind::Sinks |
                                Model::FreezeKind::Propagations)
                            .to_json(ExportOriginsMode::Always)),
      test::parse_json(R"#({
        "freeze": [ "generations", "parameter_sources", "propagation", "sinks" ],
        "method": "LData;.method:(LData;LData;)V"
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
          {},
          /* frozen */ {},
          /* generations */
          {{AccessPath(Root(Root::Kind::Argument, 2)),
            test::make_leaf_taint_config(
                context.kind_factory->get("source_kind"))}}));
  EXPECT_EQ(
      Model(
          method,
          context,
          {},
          /* frozen */ {},
          /* generations */
          {{AccessPath(Root(Root::Kind::Argument, 2)),
            test::make_leaf_taint_config(
                context.kind_factory->get("source_kind"))}})
          .to_json(ExportOriginsMode::Always),
      test::parse_json(R"#({
      "method": "LData;.method:(LData;LData;)V",
      "generations": [
        {
          "port": "Argument(2)",
          "taint": [
            {
              "call_info": { "call_kind": "Declaration" },
              "kinds": [
                {
                  "kind": "source_kind",
                  "origins": [{"method": "LData;.method:(LData;LData;)V", "port": "Argument(2)"}],
                }
              ]
            }
          ]
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
          {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */
          {{AccessPath(Root(Root::Kind::Argument, 1)),
            test::make_leaf_taint_config(
                context.kind_factory->get("source_kind"))}}));
  EXPECT_EQ(
      Model(
          method,
          context,
          {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */
          {{AccessPath(Root(Root::Kind::Argument, 1)),
            test::make_leaf_taint_config(
                context.kind_factory->get("source_kind"))}})
          .to_json(ExportOriginsMode::Always),
      test::parse_json(R"#({
      "method": "LData;.method:(LData;LData;)V",
      "parameter_sources": [
        {
          "port": "Argument(1)",
          "taint": [
            {
              "call_info": { "call_kind": "Declaration" },
              "kinds": [
                {
                  "kind": "source_kind",
                  "origins": [{"method": "LData;.method:(LData;LData;)V", "port": "Argument(1)"}],
                }
              ]
            }
          ]
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
          {},
          /* frozen */ {},
          /* generations */
          {{AccessPath(Root(Root::Kind::Return)),
            test::make_leaf_taint_config(
                context.kind_factory->get("source_kind"))}},
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
          {},
          /* frozen */ {},
          /* generations */
          {{AccessPath(Root(Root::Kind::Return)),
            test::make_leaf_taint_config(
                context.kind_factory->get("source_kind"))}},
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
          {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */
          {{AccessPath(Root(Root::Kind::Argument, 1)),
            test::make_leaf_taint_config(
                context.kind_factory->get("source_kind"))}}));

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
          {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */
          {
              PropagationConfig(
                  /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
                  /* kind */ context.kind_factory->local_return(),
                  /* output_paths */
                  PathTreeDomain{{Path{}, CollapseDepth::zero()}},
                  /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* user_features */ FeatureSet::bottom()),
              PropagationConfig(
                  /* input_path */ AccessPath(Root(Root::Kind::Argument, 2)),
                  /* kind */ context.kind_factory->local_argument(0),
                  /* output_paths */
                  PathTreeDomain{{Path{}, CollapseDepth::zero()}},
                  /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* user_features */ FeatureSet::bottom()),
          }));
  EXPECT_EQ(
      test::sorted_json(
          Model(
              method,
              context,
              {},
              /* frozen */ {},
              /* generations */ {},
              /* parameter_sources */ {},
              /* sinks */ {},
              /* propagations */
              {
                  PropagationConfig(
                      /* input_path */ AccessPath(
                          Root(Root::Kind::Argument, 1)),
                      /* kind */ context.kind_factory->local_argument(0),
                      /* output_paths */
                      PathTreeDomain{{Path{}, CollapseDepth::zero()}},
                      /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                      /* locally_inferred_features */
                      FeatureMayAlwaysSet::bottom(),
                      /* user_features */ FeatureSet::bottom()),
                  PropagationConfig(
                      /* input_path */ AccessPath(
                          Root(Root::Kind::Argument, 2)),
                      /* kind */ context.kind_factory->local_argument(0),
                      /* output_paths */
                      PathTreeDomain{
                          {Path{PathElement::field("x")},
                           CollapseDepth::zero()},
                          {Path{PathElement::field("y")},
                           CollapseDepth::zero()}},
                      /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                      /* locally_inferred_features */
                      FeatureMayAlwaysSet::bottom(),
                      /* user_features */ FeatureSet::bottom()),
              })
              .to_json(ExportOriginsMode::Always)),
      test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
        "propagation": [
          {
            "input": "Argument(1)",
            "output": [
              {
                "call_info": {
                  "call_kind": "Propagation",
                  "port": "Argument(0)"
                },
                "kinds": [
                  {
                    "kind": "LocalArgument(0)",
                    "output_paths": { "": 0 }
                  }
                ]
              }
            ]
          },
          {
            "input": "Argument(2)",
            "output": [
              {
                "call_info": {
                  "call_kind": "Propagation",
                  "port": "Argument(0)"
                },
                "kinds": [
                  {
                    "kind": "LocalArgument(0)",
                    "output_paths": { ".x": 0, ".y": 0 }
                  }
                ]
              }
            ]
          }
        ]
      })#"));

  const auto* kind1 = context.kind_factory->get("Kind1");
  const auto* kind2 = context.kind_factory->get_partial("Kind2", "a");
  const auto* kind3 = context.kind_factory->get("Kind3");
  EXPECT_EQ(
      Model::from_json(
          method,
          test::parse_json(R"#({
          "propagation": [
              {
                "input": "Argument(1)",
                "output": "Return"
              }
            ],
          "sanitizers": [
            {"sanitize": "sources", "kinds": [{"kind": "Kind1"}]},
            {"sanitize": "propagations"}
          ]
          })#"),
          context),
      Model(
          method,
          context,
          {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */
          {
              PropagationConfig(
                  /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
                  /* kind */ context.kind_factory->local_return(),
                  /* output_paths */
                  PathTreeDomain{{Path{}, CollapseDepth::zero()}},
                  /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* user_features */ FeatureSet::bottom()),
          },
          /* global_sanitizers */
          {Sanitizer(SanitizerKind::Sources, KindSetAbstractDomain({kind1})),
           Sanitizer(
               SanitizerKind::Propagations, KindSetAbstractDomain::top())}));
  EXPECT_EQ(
      test::sorted_json(
          Model(
              method,
              context,
              {},
              /* frozen */ {},
              /* generations */ {},
              /* parameter_sources */ {},
              /* sinks */ {},
              /* propagations */ {},
              /* global_sanitizers */
              {Sanitizer(
                   SanitizerKind::Sources, KindSetAbstractDomain({kind1})),
               Sanitizer(
                   SanitizerKind::Sinks,
                   KindSetAbstractDomain({kind1, kind2}))})
              .to_json(ExportOriginsMode::Always)),
      test::sorted_json(test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
        "sanitizers": [
          {"sanitize": "sources", "kinds": [{"kind": "Kind1"}]},
          {"sanitize": "sinks", "kinds": [{"kind": "Kind1"}, {"kind": "Partial:Kind2:a"}]}
        ]
      })#")));
  EXPECT_EQ(
      Model::from_json(
          method,
          test::parse_json(R"#({
          "propagation": [
              {
                "input": "Argument(1)",
                "output": "Return"
              }
            ],
          "sanitizers": [
            {"sanitize": "sources", "port": "Return", "kinds": [{"kind": "Kind1"}]},
            {"sanitize": "propagations"},
            {"sanitize": "sinks", "port": "Argument(0)", "kinds": [{"kind": "Kind3"}]}
          ]
          })#"),
          context),
      Model(
          method,
          context,
          {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */
          {
              PropagationConfig(
                  /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
                  /* kind */ context.kind_factory->local_return(),
                  /* output_paths */
                  PathTreeDomain{{Path{}, CollapseDepth::zero()}},
                  /* inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* user_features */ FeatureSet::bottom()),
          },
          /* global_sanitizers */
          {Sanitizer(
              SanitizerKind::Propagations, KindSetAbstractDomain::top())},
          /* port_sanitizers */
          {{Root(Root::Kind::Return),
            SanitizerSet(Sanitizer(
                SanitizerKind::Sources, KindSetAbstractDomain({kind1})))},
           {Root(Root::Kind::Argument, 0),
            SanitizerSet(Sanitizer(
                SanitizerKind::Sinks, KindSetAbstractDomain({kind3})))}}));
  EXPECT_EQ(
      test::sorted_json(
          Model(
              method,
              context,
              {},
              /* frozen */ {},
              /* generations */ {},
              /* parameter_sources */ {},
              /* sinks */ {},
              /* propagations */ {},
              /* global_sanitizers */
              {Sanitizer(
                  SanitizerKind::Sinks, KindSetAbstractDomain({kind1, kind2}))},
              /* port_sanitizers */
              {{Root(Root::Kind::Argument, 1),
                SanitizerSet(Sanitizer(
                    SanitizerKind::Sources, KindSetAbstractDomain::top()))},
               {Root(Root::Kind::Argument, 2),
                SanitizerSet(Sanitizer(
                    SanitizerKind::Sinks,
                    KindSetAbstractDomain({kind1, kind3})))}})
              .to_json(ExportOriginsMode::Always)),
      test::sorted_json(test::parse_json(R"#({
      "method": "LData;.method:(LData;LData;)V",
      "sanitizers": [
        {"sanitize": "sinks", "kinds": [{"kind": "Kind1"}, {"kind": "Partial:Kind2:a"}]},
        {"sanitize": "sinks", "port": "Argument(2)", "kinds": [{"kind": "Kind1"}, {"kind": "Kind3"}]},
        {"sanitize": "sources", "port": "Argument(1)"}
      ]
    })#")));

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
          {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */
          {
              {AccessPath(Root(Root::Kind::Argument, 2)),
               test::make_leaf_taint_config(
                   context.kind_factory->get("first_sink"))},
          }));
  EXPECT_EQ(
      test::sorted_json(Model(
                            method,
                            context,
                            {},
                            /* frozen */ {},
                            /* generations */ {},
                            /* parameter_sources */ {},
                            /* sinks */
                            {
                                {AccessPath(Root(Root::Kind::Argument, 2)),
                                 test::make_leaf_taint_config(
                                     context.kind_factory->get("first_sink"))},
                            })
                            .to_json(ExportOriginsMode::Always)),
      test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
        "sinks": [
          {
            "port": "Argument(2)",
            "taint": [
              {
                "call_info": { "call_kind": "Declaration" },
                "kinds": [
                  {
                    "kind": "first_sink",
                    "origins": [{"method": "LData;.method:(LData;LData;)V", "port": "Argument(2)"}],
                  }
                ]
              }
            ]
          }
        ]
      })#"));
  EXPECT_EQ(
      test::sorted_json(Model(
                            method,
                            context,
                            {},
                            /* frozen */ {},
                            /* generations */ {},
                            /* parameter_sources */ {},
                            /* sinks */
                            {
                                {AccessPath(Root(Root::Kind::Argument, 2)),
                                 test::make_leaf_taint_config(
                                     context.kind_factory->get("first_sink"))},
                            })
                            .to_json(ExportOriginsMode::OnlyOnOrigins)),
      test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
        "sinks": [
          {
            "port": "Argument(2)",
            "taint": [
              {
                "call_info": { "call_kind": "Declaration" },
                "kinds": [
                  {
                    "kind": "first_sink"
                  }
                ]
              }
            ]
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
          {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* global_sanitizers */ {},
          /* port_sanitizers */ {},
          /* attach_to_sources */
          {{Root(Root::Kind::Argument, 1),
            FeatureSet{context.feature_factory->get("via-method")}}}));
  EXPECT_EQ(
      test::sorted_json(
          Model(
              method,
              context,
              {},
              /* frozen */ {},
              /* generations */ {},
              /* parameter_sources */ {},
              /* sinks */ {},
              /* propagations */ {},
              /* global_sanitizers */ {},
              /* port_sanitizers */ {},
              /* attach_to_sources */
              {{Root(Root::Kind::Argument, 1),
                FeatureSet{context.feature_factory->get("via-method")}}})
              .to_json(ExportOriginsMode::Always)),
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
          {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* global_sanitizers */ {},
          /* port_sanitizers */ {},
          /* attach_to_sources */ {},
          /* attach_to_sinks */
          {{Root(Root::Kind::Argument, 1),
            FeatureSet{context.feature_factory->get("via-method")}}}));
  EXPECT_EQ(
      test::sorted_json(
          Model(
              method,
              context,
              {},
              /* frozen */ {},
              /* generations */ {},
              /* parameter_sources */ {},
              /* sinks */ {},
              /* propagations */ {},
              /* global_sanitizers */ {},
              /* port_sanitizers */ {},
              /* attach_to_sources */ {},
              /* attach_to_sinks */
              {{Root(Root::Kind::Argument, 1),
                FeatureSet{context.feature_factory->get("via-method")}}})
              .to_json(ExportOriginsMode::Always)),
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
          {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* global_sanitizers */ {},
          /* port_sanitizers */ {},
          /* attach_to_sources */ {},
          /* attach_to_sinks */ {},
          /* attach_to_propagations */
          {{Root(Root::Kind::Argument, 1),
            FeatureSet{context.feature_factory->get("via-method")}}}));
  EXPECT_EQ(
      test::sorted_json(
          Model(
              method,
              context,
              {},
              /* frozen */ {},
              /* generations */ {},
              /* parameter_sources */ {},
              /* sinks */ {},
              /* propagations */ {},
              /* global_sanitizers */ {},
              /* port_sanitizers */ {},
              /* attach_to_sources */ {},
              /* attach_to_sinks */ {},
              /* attach_to_propagations */
              {{Root(Root::Kind::Argument, 1),
                FeatureSet{context.feature_factory->get("via-method")}}})
              .to_json(ExportOriginsMode::Always)),
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
          {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* global_sanitizers */ {},
          /* port_sanitizers */ {},
          /* attach_to_sources */ {},
          /* attach_to_sinks */ {},
          /* attach_to_propagations */ {},
          /* add_features_to_arguments */
          {{Root(Root::Kind::Argument, 1),
            FeatureSet{context.feature_factory->get("via-method")}}}));
  EXPECT_EQ(
      test::sorted_json(
          Model(
              method,
              context,
              {},
              /* frozen */ {},
              /* generations */ {},
              /* parameter_sources */ {},
              /* sinks */ {},
              /* propagations */ {},
              /* global_sanitizers */ {},
              /* port_sanitizers */ {},
              /* attach_to_sources */ {},
              /* attach_to_sinks */ {},
              /* attach_to_propagations */ {},
              /* add_features_to_arguments */
              {{Root(Root::Kind::Argument, 1),
                FeatureSet{context.feature_factory->get("via-method")}}})
              .to_json(ExportOriginsMode::Always)),
      test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
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
            "inline_as_getter": "Argument(1).foo"
          })#"),
          context),
      Model(
          method,
          context,
          {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* global_sanitizers */ {},
          /* port_sanitizers */ {},
          /* attach_to_sources */ {},
          /* attach_to_sinks */ {},
          /* attach_to_propagations */ {},
          /* add_features_to_arguments */ {},
          /* inline_as_getter */
          AccessPathConstantDomain(AccessPath(
              Root(Root::Kind::Argument, 1),
              Path{PathElement::field("foo")}))));
  EXPECT_EQ(
      test::sorted_json(Model(
                            method,
                            context,
                            {},
                            /* frozen */ {},
                            /* generations */ {},
                            /* parameter_sources */ {},
                            /* sinks */ {},
                            /* propagations */ {},
                            /* global_sanitizers */ {},
                            /* port_sanitizers */ {},
                            /* attach_to_sources */ {},
                            /* attach_to_sinks */ {},
                            /* attach_to_propagations */ {},
                            /* add_features_to_arguments */ {},
                            /* inline_as_getter */
                            AccessPathConstantDomain(AccessPath(
                                Root(Root::Kind::Argument, 1),
                                Path{PathElement::field("foo")})))
                            .to_json(ExportOriginsMode::Always)),
      test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
        "inline_as_getter": "Argument(1).foo"
      })#"));

  EXPECT_EQ(
      Model::from_json(
          method,
          test::parse_json(R"#({
            "inline_as_setter": {
              "target": "Argument(0).foo",
              "value": "Argument(1)",
            }
          })#"),
          context),
      Model(
          method,
          context,
          {},
          /* frozen */ {},
          /* generations */ {},
          /* parameter_sources */ {},
          /* sinks */ {},
          /* propagations */ {},
          /* global_sanitizers */ {},
          /* port_sanitizers */ {},
          /* attach_to_sources */ {},
          /* attach_to_sinks */ {},
          /* attach_to_propagations */ {},
          /* add_features_to_arguments */ {},
          /* inline_as_getter */ AccessPathConstantDomain::bottom(),
          /* inline_as_setter */
          SetterAccessPathConstantDomain(SetterAccessPath(
              /* target */ AccessPath(
                  Root(Root::Kind::Argument, 0),
                  Path{PathElement::field("foo")}),
              /* value */ AccessPath(Root(Root::Kind::Argument, 1))))));
  EXPECT_EQ(
      test::sorted_json(
          Model(
              method,
              context,
              {},
              /* frozen */ {},
              /* generations */ {},
              /* parameter_sources */ {},
              /* sinks */ {},
              /* propagations */ {},
              /* global_sanitizers */ {},
              /* port_sanitizers */ {},
              /* attach_to_sources */ {},
              /* attach_to_sinks */ {},
              /* attach_to_propagations */ {},
              /* add_features_to_arguments */ {},
              /* inline_as_getter */ AccessPathConstantDomain::bottom(),
              /* inline_as_setter */
              SetterAccessPathConstantDomain(SetterAccessPath(
                  /* target */ AccessPath(
                      Root(Root::Kind::Argument, 0),
                      Path{PathElement::field("foo")}),
                  /* value */ AccessPath(Root(Root::Kind::Argument, 1))))

                  )
              .to_json(ExportOriginsMode::Always)),
      test::parse_json(R"#({
        "method": "LData;.method:(LData;LData;)V",
        "inline_as_setter": {
          "target": "Argument(0).foo",
          "value": "Argument(1)"
        }
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
      Rule::KindSet{context.kind_factory->get("first_source")},
      Rule::KindSet{context.kind_factory->get("first_sink")},
      /* transforms */ nullptr);
  EXPECT_EQ(
      test::sorted_json(
          Model(
              method,
              context,
              {},
              /* frozen */ {},
              /* generations */
              std::vector<std::pair<AccessPath, TaintConfig>>{},
              /* parameter_sources */ {},
              /* sinks */ {},
              /* propagations */ {},
              /* global_sanitizers */ {},
              /* port_sanitizers */ {},
              /* attach_to_sources */ {},
              /* attach_to_sinks */ {},
              /* attach_to_propagations */ {},
              /* add_features_to_arguments */ {},
              /* inline_as_getter */ AccessPathConstantDomain::bottom(),
              /* inline_as_setter */ SetterAccessPathConstantDomain::bottom(),
              /* model_generators */ {},
              IssueSet{Issue(
                  /* source */ Taint{test::make_leaf_taint_config(
                      context.kind_factory->get("first_source"))},
                  /* sink */
                  Taint{test::make_leaf_taint_config(
                      context.kind_factory->get("first_sink"))},
                  rule.get(),
                  /* callee */ "LClass;.someMethod:()V",
                  /* sink_index */ 1,
                  context.positions->get("Data.java", 1))})
              .to_json(ExportOriginsMode::Always)),
      test::parse_json(R"#({
      "method": "LData;.method:(LData;LData;)V",
      "issues": [
        {
          "rule": 1,
          "position": {
            "path": "Data.java",
            "line": 1
          },
        "callee" : "LClass;.someMethod:()V",
        "sink_index" : "1",
          "sources": [
            {
              "call_info": { "call_kind": "Declaration" },
              "kinds": [ {"kind": "first_source"} ]
            }
          ],
          "sinks": [
            {
              "call_info": { "call_kind": "Declaration" },
              "kinds": [ {"kind": "first_sink"} ]
            }
          ]
        }
      ]
    })#"));
}

TEST_F(JsonTest, FieldModel) {
  Scope scope;
  const auto* dex_field = redex::create_field(
      scope,
      /* class_name */ "LBase;",
      /* fields */
      {"field1", type::java_lang_String()});

  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  const auto* field = context.fields->get(dex_field);
  const auto* source_kind = context.kind_factory->get("TestSource");
  const auto* source_kind2 = context.kind_factory->get("TestSource2");
  const auto* sink_kind = context.kind_factory->get("TestSink");
  const auto* feature = context.feature_factory->get("test-feature");

  EXPECT_THROW(
      FieldModel::from_json(field, test::parse_json(R"(1)"), context),
      JsonValidationError);
  EXPECT_EQ(
      FieldModel::from_json(field, test::parse_json(R"({})"), context),
      FieldModel(field));

  // Field taint frames must be leaf frames
  EXPECT_THROW(
      FieldModel::from_json(
          field,
          test::parse_json(R"("sources": [
            {"kind": "TestSource", "callee_port": "Return"},
          ]})"),
          context),
      JsonValidationError);
  EXPECT_THROW(
      FieldModel::from_json(
          field,
          test::parse_json(R"("sinks": [
            {"kind": "TestSource", "callee": "LClass;.someMethod:()V"},
          ]})"),
          context),
      JsonValidationError);

  // Test from_json
  EXPECT_EQ(
      FieldModel::from_json(
          field,
          test::parse_json(R"({"sources": [
            {"kind": "TestSource"},
            {"kind": "TestSource2", "features": ["test-feature"]}
          ]})"),
          context),
      FieldModel(
          field,
          /* sources */
          {test::make_leaf_taint_config(source_kind),
           test::make_taint_config(
               source_kind2,
               test::FrameProperties{
                   .inferred_features = FeatureMayAlwaysSet::bottom(),
                   .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                   .user_features = FeatureSet{feature}})},
          /* sinks */ {}));
  EXPECT_EQ(
      FieldModel::from_json(
          field,
          test::parse_json(R"({"sinks": [
            {"kind": "TestSink"},
          ]})"),
          context),
      FieldModel(
          field,
          /* sources */ {},
          /* sinks */
          {test::make_leaf_taint_config(sink_kind)}));

  // Test to_json
  EXPECT_EQ(
      FieldModel(field).to_json(ExportOriginsMode::Always),
      test::parse_json(R"({"field": "LBase;.field1:Ljava/lang/String;"})"));
  EXPECT_EQ(
      FieldModel(
          field,
          /* sources */ {},
          /* sinks */
          {test::make_taint_config(
              sink_kind,
              test::FrameProperties{
                  .inferred_features = FeatureMayAlwaysSet::bottom(),
                  .user_features = FeatureSet{feature}})})
          .to_json(ExportOriginsMode::Always),
      test::parse_json(R"({
        "field": "LBase;.field1:Ljava/lang/String;",
        "sinks": [
          {
            "kind": "TestSink",
            "always_features": ["test-feature"],
            "origins": [{"field": "LBase;.field1:Ljava/lang/String;"}],
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

TEST_F(JsonTest, LifecycleMethods) {
  LifecycleMethods methods;
  methods.add_methods_from_json(test::parse_json(R"([{
        "base_class_name": "Landroidx/fragment/app/FragmentActivity;",
        "method_name": "activity_lifecycle_wrapper",
        "callees": [
          {
            "method_name": "onCreate",
            "return_type": "V",
            "argument_types": []
          }
        ]
      }])"));

  // Re-using name "activity_lifecycle_wrapper" should fail.
  EXPECT_THROW(
      methods.add_methods_from_json(test::parse_json(R"([{
        "base_class_name": "Landroidx/fragment/app/FragmentActivity2;",
        "method_name": "activity_lifecycle_wrapper",
        "callees": [
          {
            "method_name": "onCreate",
            "return_type": "V",
            "argument_types": []
          }
        ]
      }])")),
      JsonValidationError);

  // Re-using name "mymethodname" within the same JSON array should fail.
  EXPECT_THROW(
      methods.add_methods_from_json(test::parse_json(R"([{
        "base_class_name": "Landroidx/fragment/app/FragmentActivity3;",
        "method_name": "mymethodname",
        "callees": [
          {
            "method_name": "onCreate",
            "return_type": "V",
            "argument_types": []
          }
        ]
      },
      {
        "base_class_name": "Landroidx/fragment/app/FragmentActivity4;",
        "method_name": "mymethodname",
        "callees": [
          {
            "method_name": "onCreate",
            "return_type": "V",
            "argument_types": []
          }
        ]
      }])")),
      JsonValidationError);
}

TEST_F(JsonTest, CallEffectModel) {
  Scope scope;
  auto* dex_entry_method = redex::create_void_method(
      scope,
      /* class_name */ "LEntry;",
      /* method_name */ "method");
  auto* dex_exit_method = redex::create_void_method(
      scope,
      /* class_name */ "LExit;",
      /* method_name */ "method");

  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);
  auto* entry_method = context.methods->get(dex_entry_method);
  auto* exit_method = context.methods->get(dex_exit_method);

  Model effect_source_model(entry_method, context);
  effect_source_model.add_call_effect_source(
      AccessPath(Root(Root::Kind::CallEffectCallChain)),
      test::make_leaf_taint_config(
          context.kind_factory->get("CallChainOrigin")));

  EXPECT_EQ(
      test::sorted_json(effect_source_model.to_json(ExportOriginsMode::Always)),
      test::parse_json(R"#({
      "effect_sources": [
        {
          "port": "call-chain",
          "taint": [
            {
              "call_info": {
                "call_kind": "Declaration"
              },
              "kinds": [
                {
                  "kind": "CallChainOrigin",
                  "origins": [{"method": "LEntry;.method:()V", "port": "call-chain"}],
                }
              ]
            }
          ]
        }
      ],
      "method": "LEntry;.method:()V"
    })#"));

  Model effect_sink_model(exit_method, context);
  effect_sink_model.add_call_effect_sink(
      AccessPath(Root(Root::Kind::CallEffectCallChain)),
      test::make_leaf_taint_config(context.kind_factory->get("CallChainSink")));
  EXPECT_EQ(
      test::sorted_json(effect_sink_model.to_json(ExportOriginsMode::Always)),
      test::parse_json(R"#({
      "effect_sinks": [
        {
          "port": "call-chain",
          "taint": [
            {
              "call_info": {
                "call_kind": "Declaration"
              },
              "kinds": [
                {
                  "kind": "CallChainSink",
                  "origins": [{"method": "LExit;.method:()V", "port": "call-chain"}],
                }
              ]
            }
          ]
        }
      ],
      "method": "LExit;.method:()V"
    })#"));

  auto rule = std::make_unique<SourceSinkRule>(
      /* name */ "Rule",
      /* code */ 1,
      /* description */ "",
      Rule::KindSet{context.kind_factory->get("CallChainOrigin")},
      Rule::KindSet{context.kind_factory->get("CallChainSink")},
      /* transforms */ nullptr);

  auto source = effect_source_model.call_effect_sources()
                    .read(Root(Root::Kind::CallEffectCallChain))
                    .collapse(/* broadening_features */ {});
  auto sink = effect_sink_model.call_effect_sinks()
                  .read(Root(Root::Kind::CallEffectCallChain))
                  .collapse(/* broadening_features */ {});

  EXPECT_EQ(
      test::sorted_json(
          Model(
              entry_method,
              context,
              {},
              /* frozen */ {},
              /* generations */
              std::vector<std::pair<AccessPath, TaintConfig>>{},
              /* parameter_sources */ {},
              /* sinks */ {},
              /* propagations */ {},
              /* global_sanitizers */ {},
              /* port_sanitizers */ {},
              /* attach_to_sources */ {},
              /* attach_to_sinks */ {},
              /* attach_to_propagations */ {},
              /* add_features_to_arguments */ {},
              /* inline_as_getter */ AccessPathConstantDomain::bottom(),
              /* inline_as_setter */ SetterAccessPathConstantDomain::bottom(),
              /* model_generators */ {},
              IssueSet{Issue(
                  std::move(source),
                  std::move(sink),
                  rule.get(),
                  /* callee */ exit_method->signature(),
                  /* sink_index */ 0,
                  context.positions->get("CallEffect.java", 1))})
              .to_json(ExportOriginsMode::Always)),
      test::parse_json(R"#({
      "issues": [
        {
          "callee" : "LExit;.method:()V",
          "position": {
            "path": "CallEffect.java",
            "line": 1
          },
          "rule": 1,
          "sink_index" : "0",
          "sinks": [
            {
              "call_info": {
                "call_kind": "Declaration"
              },
              "kinds": [
                {
                  "kind": "CallChainSink",
                  "origins": [{"method": "LExit;.method:()V", "port": "call-chain"}],
                }
              ]
            }
          ],
          "sources": [
            {
              "call_info": {
                "call_kind": "Declaration"
              },
              "kinds": [
                {
                  "kind": "CallChainOrigin",
                  "origins": [{"method": "LEntry;.method:()V", "port": "call-chain"}],
                }
              ]
            }
          ]
        }
      ],
      "method": "LEntry;.method:()V"
    })#"));
}

} // namespace marianatrench

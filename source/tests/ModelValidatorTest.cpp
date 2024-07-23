/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/ModelValidator.h>
#include <mariana-trench/SourceSinkRule.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class ModelValidatorTest : public test::Test {};

TEST_F(ModelValidatorTest, ModelValidators) {
  auto context = test::make_empty_context();

  const auto* source_kind = context.kind_factory->get("TestSource");
  const auto* sink_kind = context.kind_factory->get("TestSink");
  const auto* position = context.positions->get(std::nullopt, 1);

  Scope scope;
  auto* method = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));

  SourceSinkRule rule_1(
      "rule 1",
      /* code */ 1,
      "description",
      {source_kind},
      {sink_kind},
      /* transforms */ nullptr);

  // Single issue with single source and sink kind.
  Model model;
  model.add_issue(Issue(
      /* source */ Taint{test::make_leaf_taint_config(source_kind)},
      /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
      &rule_1,
      "callee",
      /* sink_index */ 0,
      position));

  {
    // All validators pass
    std::vector<std::unique_ptr<ModelValidator>> validators;
    validators.push_back(std::make_unique<ExpectIssue>(
        /* code */ 1,
        /* source_kinds */ std::set<std::string>{},
        /* sink_kinds */ std::set<std::string>{},
        /* source_origins */ std::set<std::string>{},
        /* sink_origins */ std::set<std::string>{}));
    validators.push_back(std::make_unique<ExpectIssue>(
        /* code */ 1,
        /* source_kinds */ std::set<std::string>{"TestSource"},
        /* sink_kinds */ std::set<std::string>{},
        /* source_origins */ std::set<std::string>{},
        /* sink_origins */ std::set<std::string>{}));

    ModelValidators validator(method, std::move(validators));
    auto result = validator.validate(model).to_json();
    auto expected = test::parse_json(
        R"#({
              "method": "LClass;.one:()V",
              "validators": [
                { "valid": true, "annotation": "ExpectIssue(code=1, sourceKinds=, sinkKinds=, sourceOrigins=, sinkOrigins=)" },
                { "valid": true, "annotation": "ExpectIssue(code=1, sourceKinds=TestSource, sinkKinds=, sourceOrigins=, sinkOrigins=)" }
              ]
            })#");
    EXPECT_EQ(test::sorted_json(expected), test::sorted_json(result));
  }

  {
    // One validator fails
    std::vector<std::unique_ptr<ModelValidator>> validators;
    validators.push_back(std::make_unique<ExpectIssue>(
        /* code */ 1,
        /* source_kinds */ std::set<std::string>{},
        /* sink_kinds */ std::set<std::string>{},
        /* source_origins */ std::set<std::string>{},
        /* sink_origins */ std::set<std::string>{}));
    validators.push_back(std::make_unique<ExpectIssue>(
        /* code */ 2,
        /* source_kinds */ std::set<std::string>{},
        /* sink_kinds */ std::set<std::string>{},
        /* source_origins */ std::set<std::string>{},
        /* sink_origins */ std::set<std::string>{}));

    ModelValidators validator(method, std::move(validators));
    auto result = validator.validate(model).to_json();
    auto expected = test::parse_json(
        R"#({
              "method": "LClass;.one:()V",
              "validators": [
                { "valid": true, "annotation": "ExpectIssue(code=1, sourceKinds=, sinkKinds=, sourceOrigins=, sinkOrigins=)" },
                { "valid": false, "annotation": "ExpectIssue(code=2, sourceKinds=, sinkKinds=, sourceOrigins=, sinkOrigins=)" }
              ]
            })#");
    EXPECT_EQ(test::sorted_json(expected), test::sorted_json(result));
  }

  {
    // All validators fail (and single validator)
    std::vector<std::unique_ptr<ModelValidator>> validators;
    validators.push_back(std::make_unique<ExpectNoIssue>(ExpectIssue(
        /* code */ 1,
        /* source_kinds */ std::set<std::string>{},
        /* sink_kinds */ std::set<std::string>{},
        /* source_origins */ std::set<std::string>{},
        /* sink_origins */ std::set<std::string>{})));

    ModelValidators validator(method, std::move(validators));
    auto result = validator.validate(model).to_json();
    auto expected = test::parse_json(
        R"#({
              "method": "LClass;.one:()V",
              "validators": [
                { "valid": false, "annotation": "ExpectNoIssue(code=1, sourceKinds=, sinkKinds=, sourceOrigins=, sinkOrigins=)" }
              ]
            })#");
    EXPECT_EQ(test::sorted_json(expected), test::sorted_json(result));
  }

  {
    // All validators pass, with a different validator types
    std::vector<std::unique_ptr<ModelValidator>> validators;
    validators.push_back(std::make_unique<ExpectIssue>(
        /* code */ 1,
        /* source_kinds */ std::set<std::string>{},
        /* sink_kinds */ std::set<std::string>{},
        /* source_origins */ std::set<std::string>{},
        /* sink_origins */ std::set<std::string>{}));
    validators.push_back(std::make_unique<ExpectNoIssue>(ExpectIssue(
        /* code */ 2,
        /* source_kinds */ std::set<std::string>{},
        /* sink_kinds */ std::set<std::string>{},
        /* source_origins */ std::set<std::string>{},
        /* sink_origins */ std::set<std::string>{})));

    ModelValidators validator(method, std::move(validators));
    auto result = validator.validate(model).to_json();
    auto expected = test::parse_json(
        R"#({
              "method": "LClass;.one:()V",
              "validators": [
                { "valid": true, "annotation": "ExpectIssue(code=1, sourceKinds=, sinkKinds=, sourceOrigins=, sinkOrigins=)" },
                { "valid": true, "annotation": "ExpectNoIssue(code=2, sourceKinds=, sinkKinds=, sourceOrigins=, sinkOrigins=)" }
              ]
            })#");
    EXPECT_EQ(test::sorted_json(expected), test::sorted_json(result));
  }
}

TEST_F(ModelValidatorTest, ExpectIssueExpectNoIssue) {
  auto context = test::make_empty_context();

  const auto* source_kind = context.kind_factory->get("TestSource");
  const auto* other_source_kind = context.kind_factory->get("OtherSource");
  const auto* sink_kind = context.kind_factory->get("TestSink");
  const auto* other_sink_kind = context.kind_factory->get("OtherSink");
  const auto* position = context.positions->get(std::nullopt, 1);

  Scope scope;
  auto* method = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));
  const auto* argument0 =
      context.access_path_factory->get(AccessPath(Root::argument(0)));
  const auto* source_origin =
      context.origin_factory->string_origin("TestSourceOrigin");
  const auto* sink_origin =
      context.origin_factory->method_origin(method, argument0);

  SourceSinkRule rule_1(
      "rule 1",
      /* code */ 1,
      "description",
      {source_kind, other_source_kind},
      {sink_kind, other_sink_kind},
      /* transforms */ nullptr);

  // Single issue with single source and sink kind.
  Model model;
  model.add_issue(Issue(
      /* source */ Taint{test::make_leaf_taint_config(source_kind)},
      /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
      &rule_1,
      "callee",
      /* sink_index */ 0,
      position));

  {
    // Matches code and trivially matches source and sink kinds (empty set).
    ExpectIssue expect_issue(
        1,
        /* source_kinds */ {},
        /* sink_kinds */ {},
        /* source_origins */ {},
        /* sink_origins */ {});
    EXPECT_TRUE(expect_issue.validate(model).is_valid());
    EXPECT_FALSE(
        ExpectNoIssue(std::move(expect_issue)).validate(model).is_valid());
  }

  {
    // Matches code, sources and sinks.
    ExpectIssue expect_issue(
        1,
        {"TestSource"},
        {"TestSink"},
        /* source_origins */ {},
        /* sink_origins */ {});
    EXPECT_TRUE(expect_issue.validate(model).is_valid());
    EXPECT_FALSE(
        ExpectNoIssue(std::move(expect_issue)).validate(model).is_valid());
  }

  {
    // Fails code test
    ExpectIssue expect_issue(
        2,
        {"TestSource"},
        {"TestSink"},
        /* source_origins */ {},
        /* sink_origins */ {});
    EXPECT_FALSE(expect_issue.validate(model).is_valid());
    EXPECT_TRUE(
        ExpectNoIssue(std::move(expect_issue)).validate(model).is_valid());
  }

  {
    // Fails sink subset test (expected is a disjoint set)
    ExpectIssue expect_issue(
        1,
        {"TestSource"},
        {"OtherSink"},
        /* source_origins */ {},
        /* sink_origins */ {});
    EXPECT_FALSE(expect_issue.validate(model).is_valid());
    EXPECT_TRUE(
        ExpectNoIssue(std::move(expect_issue)).validate(model).is_valid());
  }

  {
    // Fails source subset test (expected is a disjoint set)
    ExpectIssue expect_issue(
        1,
        {"OtherSource"},
        {"TestSink"},
        /* source_origins */ {},
        /* sink_origins */ {});
    EXPECT_FALSE(expect_issue.validate(model).is_valid());
    EXPECT_TRUE(
        ExpectNoIssue(std::move(expect_issue)).validate(model).is_valid());
  }

  {
    // Fails source subset test (expected is a superset)
    ExpectIssue expect_issue(
        1,
        {"TestSource", "OtherSource"},
        {"TestSink"},
        /* source_origins */ {},
        /* sink_origins */ {});
    EXPECT_FALSE(expect_issue.validate(model).is_valid());
    EXPECT_TRUE(
        ExpectNoIssue(std::move(expect_issue)).validate(model).is_valid());
  }

  // Single issue with multiple source and sink kinds.
  model = Model();
  model.add_issue(Issue(
      /* source */
      Taint{
          test::make_leaf_taint_config(source_kind),
          test::make_leaf_taint_config(other_source_kind)},
      /* sink */
      Taint{
          test::make_leaf_taint_config(sink_kind),
          test::make_leaf_taint_config(other_sink_kind)},
      &rule_1,
      "callee",
      /* sink_index */ 0,
      position));

  {
    // Matches code, sources (subset) and sinks (subset).
    ExpectIssue expect_issue(
        1,
        {"TestSource"},
        {"TestSink"},
        /* source_origins */ {},
        /* sink_origins */ {});
    EXPECT_TRUE(expect_issue.validate(model).is_valid());
    EXPECT_FALSE(
        ExpectNoIssue(std::move(expect_issue)).validate(model).is_valid());
  }

  {
    // Matches code, sources (exact) and sinks (exact).
    ExpectIssue expect_issue(
        1,
        {"TestSource", "OtherSource"},
        {"TestSink", "OtherSink"},
        /* source_origins */ {},
        /* sink_origins */ {});
    EXPECT_TRUE(expect_issue.validate(model).is_valid());
    EXPECT_FALSE(
        ExpectNoIssue(std::move(expect_issue)).validate(model).is_valid());
  }

  {
    // Fails source subset test
    ExpectIssue expect_issue(
        1,
        {"TestSource2"},
        {"TestSink"},
        /* source_origins */ {},
        /* sink_origins */ {});
    EXPECT_FALSE(expect_issue.validate(model).is_valid());
    EXPECT_TRUE(
        ExpectNoIssue(std::move(expect_issue)).validate(model).is_valid());
  }

  // Multiple issues, each with with single source and sink kind.
  model = Model();
  model.add_issue(Issue(
      /* source */ Taint{test::make_leaf_taint_config(source_kind)},
      /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
      &rule_1,
      "callee",
      /* sink_index */ 0,
      position));
  model.add_issue(Issue(
      /* source */ Taint{test::make_leaf_taint_config(other_source_kind)},
      /* sink */ Taint{test::make_leaf_taint_config(other_sink_kind)},
      &rule_1,
      "callee2", // Different callee to prevent merging of kinds
      /* sink_index */ 0,
      position));

  {
    // Matches code, sources and sinks for one issue.
    ExpectIssue expect_issue(
        1,
        {"TestSource"},
        {"TestSink"},
        /* source_origins */ {},
        /* sink_origins */ {});
    EXPECT_TRUE(expect_issue.validate(model).is_valid());
    EXPECT_FALSE(
        ExpectNoIssue(std::move(expect_issue)).validate(model).is_valid());
  }

  {
    // Fails source/sink kind subset test across all issues.
    ExpectIssue expect_issue(
        1,
        {"TestSource", "OtherSource"},
        {"TestSink", "OtherSink"},
        /* source_origins */ {},
        /* sink_origins */ {});
    EXPECT_FALSE(expect_issue.validate(model).is_valid());
    EXPECT_TRUE(
        ExpectNoIssue(std::move(expect_issue)).validate(model).is_valid());
  }

  // Single issue with source and sink kinds and origins
  model = Model();
  model.add_issue(Issue(
      /* source */
      Taint{
          test::make_leaf_taint_config(source_kind, OriginSet{source_origin})},
      /* sink */
      Taint{test::make_leaf_taint_config(sink_kind, OriginSet{sink_origin})},
      &rule_1,
      "callee",
      /* sink_index */ 0,
      position));

  {
    // Passes source origins test
    ExpectIssue expect_issue(
        1,
        /* source_kinds */ {},
        /* sink_kinds */ {},
        /* source_origins */ {"TestSourceOrigin"},
        /* sink_origins */ {});
    EXPECT_TRUE(expect_issue.validate(model).is_valid());
    EXPECT_FALSE(
        ExpectNoIssue(std::move(expect_issue)).validate(model).is_valid());
  }

  {
    // Passes source and sink origins test
    ExpectIssue expect_issue(
        1,
        /* source_kinds */ {},
        /* sink_kinds */ {},
        /* source_origins */ {"TestSourceOrigin"},
        /* sink_origins */ {method->show()});
    EXPECT_TRUE(expect_issue.validate(model).is_valid());
    EXPECT_FALSE(
        ExpectNoIssue(std::move(expect_issue)).validate(model).is_valid());
  }

  {
    // Fails sink origin test (passes source origins test)
    ExpectIssue expect_issue(
        1,
        /* source_kinds */ {},
        /* sink_kinds */ {},
        /* source_origins */ {"TestSourceOrigin"},
        /* sink_origins */ {"InvalidSinkorigin"});
    EXPECT_FALSE(expect_issue.validate(model).is_valid());
    EXPECT_TRUE(
        ExpectNoIssue(std::move(expect_issue)).validate(model).is_valid());
  }
}

} // namespace marianatrench

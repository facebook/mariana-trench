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
        1,
        /* source_kinds */ std::set<std::string>{},
        /* sink_kinds */ std::set<std::string>{}));
    validators.push_back(std::make_unique<ExpectIssue>(
        1,
        /* source_kinds */ std::set<std::string>{"TestSource"},
        /* sink_kinds */ std::set<std::string>{}));

    ModelValidators validator(std::move(validators));
    EXPECT_TRUE(validator.validate(model));
  }

  {
    // One validator fails
    std::vector<std::unique_ptr<ModelValidator>> validators;
    validators.push_back(std::make_unique<ExpectIssue>(
        1,
        /* source_kinds */ std::set<std::string>{},
        /* sink_kinds */ std::set<std::string>{}));
    validators.push_back(std::make_unique<ExpectIssue>(
        2,
        /* source_kinds */ std::set<std::string>{},
        /* sink_kinds */ std::set<std::string>{}));

    ModelValidators validator(std::move(validators));
    EXPECT_FALSE(validator.validate(model));
  }

  {
    // All validators fail (and single validator)
    std::vector<std::unique_ptr<ModelValidator>> validators;
    validators.push_back(std::make_unique<ExpectNoIssue>(1));

    ModelValidators validator(std::move(validators));
    EXPECT_FALSE(validator.validate(model));
  }

  {
    // All validators pass, with a different validator types
    std::vector<std::unique_ptr<ModelValidator>> validators;
    validators.push_back(std::make_unique<ExpectIssue>(
        1,
        /* source_kinds */ std::set<std::string>{},
        /* sink_kinds */ std::set<std::string>{}));
    validators.push_back(std::make_unique<ExpectNoIssue>(2));

    ModelValidators validator(std::move(validators));
    EXPECT_TRUE(validator.validate(model));
  }
}

TEST_F(ModelValidatorTest, ExpectIssue) {
  auto context = test::make_empty_context();

  const auto* source_kind = context.kind_factory->get("TestSource");
  const auto* other_source_kind = context.kind_factory->get("OtherSource");
  const auto* sink_kind = context.kind_factory->get("TestSink");
  const auto* other_sink_kind = context.kind_factory->get("OtherSink");
  const auto* position = context.positions->get(std::nullopt, 1);

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
        /* sink_kinds */ {});
    EXPECT_TRUE(expect_issue.validate(model));
  }

  {
    // Matches code, sources and sinks.
    ExpectIssue expect_issue(1, {"TestSource"}, {"TestSink"});
    EXPECT_TRUE(expect_issue.validate(model));
  }

  {
    // Fails code test
    ExpectIssue expect_issue(2, {"TestSource"}, {"TestSink"});
    EXPECT_FALSE(expect_issue.validate(model));
  }

  {
    // Fails sink subset test (expected is a disjoint set)
    ExpectIssue expect_issue(1, {"TestSource"}, {"OtherSink"});
    EXPECT_FALSE(expect_issue.validate(model));
  }

  {
    // Fails source subset test (expected is a disjoint set)
    ExpectIssue expect_issue(1, {"OtherSource"}, {"TestSink"});
    EXPECT_FALSE(expect_issue.validate(model));
  }

  {
    // Fails source subset test (expected is a superset)
    ExpectIssue expect_issue(1, {"TestSource", "OtherSource"}, {"TestSink"});
    EXPECT_FALSE(expect_issue.validate(model));
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
    ExpectIssue expect_issue(1, {"TestSource"}, {"TestSink"});
    EXPECT_TRUE(expect_issue.validate(model));
  }

  {
    // Matches code, sources (exact) and sinks (exact).
    ExpectIssue expect_issue(
        1, {"TestSource", "OtherSource"}, {"TestSink", "OtherSink"});
    EXPECT_TRUE(expect_issue.validate(model));
  }

  {
    // Fails source subset test
    ExpectIssue expect_issue(1, {"TestSource2"}, {"TestSink"});
    EXPECT_FALSE(expect_issue.validate(model));
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
    ExpectIssue expect_issue(1, {"TestSource"}, {"TestSink"});
    EXPECT_TRUE(expect_issue.validate(model));
  }

  {
    // Fails source/sink kind subset test across all issues.
    ExpectIssue expect_issue(
        1, {"TestSource", "OtherSource"}, {"TestSink", "OtherSink"});
    EXPECT_FALSE(expect_issue.validate(model));
  }
}

TEST_F(ModelValidatorTest, ExpectNoIssue) {
  auto context = test::make_empty_context();

  const auto* source_kind = context.kind_factory->get("TestSource");
  const auto* sink_kind = context.kind_factory->get("TestSink");
  const auto* position = context.positions->get(std::nullopt, 1);

  SourceSinkRule rule_1(
      "rule 1",
      /* code */ 1,
      "description",
      {source_kind},
      {sink_kind},
      /* transforms */ nullptr);

  // Empty model
  Model model;

  {
    // Matches no issues (regardless of code)
    ExpectNoIssue expect_no_issue(/* code */ -1);
    EXPECT_TRUE(expect_no_issue.validate(model));
  }
  {
    // Matches no issues with code "1"
    ExpectNoIssue expect_no_issue(/* code */ 1);
    EXPECT_TRUE(expect_no_issue.validate(model));
  }

  // Single issue with single source and sink kind.
  model.add_issue(Issue(
      /* source */ Taint{test::make_leaf_taint_config(source_kind)},
      /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
      &rule_1,
      "callee",
      /* sink_index */ 0,
      position));

  {
    // Fails "no issues" check
    ExpectNoIssue expect_no_issue(/* code */ -1);
    EXPECT_FALSE(expect_no_issue.validate(model));
  }

  {
    // Fails "no issues" with code "1" check
    ExpectNoIssue expect_no_issue(/* code */ 1);
    EXPECT_FALSE(expect_no_issue.validate(model));
  }

  {
    // Passes "no issues" with code "2" check
    ExpectNoIssue expect_no_issue(/* code */ 2);
    EXPECT_TRUE(expect_no_issue.validate(model));
  }
}

} // namespace marianatrench

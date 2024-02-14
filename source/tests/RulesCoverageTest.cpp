/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/RulesCoverage.h>
#include <mariana-trench/SourceSinkRule.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class RulesCoverageTest : public test::Test {};

TEST_F(RulesCoverageTest, TestCoverageInfo) {
  DexStore store("stores");
  auto context = test::make_context(store);

  const auto* source1 = context.kind_factory->get("Source1");
  const auto* source2 = context.kind_factory->get("Source2");
  const auto* sink1 = context.kind_factory->get("Sink1");
  const auto* transform1 =
      context.transforms_factory->create_transform("Transform1");
  const auto* transform_list1 =
      context.transforms_factory->create({"Transform1"}, context);

  auto* partial_sink_a =
      context.kind_factory->get_partial("PartialSink1", "labelA");
  auto* partial_sink_b =
      context.kind_factory->get_partial("ParitialSink1", "labelB");

  auto rules = Rules(context);
  rules.add(
      context,
      std::make_unique<SourceSinkRule>(
          /* name */ "Rule1",
          /* code */ 1,
          /* description */ "Simple Rule",
          /* source_kinds */ Rule::KindSet{source1},
          /* sink_kinds */ Rule::KindSet{sink1},
          /* transforms */ nullptr));
  rules.add(
      context,
      std::make_unique<SourceSinkRule>(
          /* name */ "Rule2",
          /* code */ 2,
          /* description */ "Rule with Transforms",
          /* source_kinds */ Rule::KindSet{source1},
          /* sink_kinds */ Rule::KindSet{sink1},
          /* transforms */ transform_list1));
  rules.add(
      context,
      std::make_unique<MultiSourceMultiSinkRule>(
          /* name */ "Rule3",
          /* code */ 3,
          /* description */ "Multi-source Rule",
          /* multi_source_kinds */
          MultiSourceMultiSinkRule::MultiSourceKindsByLabel{
              {"labelA", {source1}}, {"labelB", {source2}}},
          /* partial_sink_kinds */
          MultiSourceMultiSinkRule::PartialKindSet{
              partial_sink_a, partial_sink_b}));

  // Trivial case: No coverage.
  EXPECT_EQ(
      RulesCoverage::create(
          rules,
          /* used_sources */ {},
          /* used_sinks */ {},
          /* used_transforms */ {}),
      (RulesCoverage{
          .covered_rules = {},
          .non_covered_rule_codes = {1, 2, 3},
      }));

  // Simple source-sink rule (no transforms).
  EXPECT_EQ(
      RulesCoverage::create(
          rules,
          /* used_sources */ {source1},
          /* used_sinks */ {sink1},
          /* used_transforms */ {}),
      (RulesCoverage{
          .covered_rules =
              {{1,
                CoveredRule{
                    .code = 1,
                    .used_sources = {source1},
                    .used_sinks = {sink1},
                    .used_transforms = {}}}},
          .non_covered_rule_codes = {2, 3},
      }));

  // Source-sink rule with transforms.
  EXPECT_EQ(
      RulesCoverage::create(
          rules,
          /* used_sources */ {source1},
          /* used_sinks */ {sink1},
          /* used_transforms */ {transform1}),
      (RulesCoverage{
          .covered_rules =
              {{1,
                CoveredRule{
                    .code = 1,
                    .used_sources = {source1},
                    .used_sinks = {sink1},
                    .used_transforms = {}}},
               {2,
                CoveredRule{
                    .code = 2,
                    .used_sources = {source1},
                    .used_sinks = {sink1},
                    .used_transforms = {transform1}}}},
          .non_covered_rule_codes = {3},
      }));

  // Multi-source rule with partial source/sink coverage.
  // For these rules, *both* branches/labels must have sources/sinks in the
  // input.
  EXPECT_EQ(
      RulesCoverage::create(
          rules,
          /* used_sources */ {source1},
          /* used_sinks */ {partial_sink_a},
          /* used_transforms */ {}),
      (RulesCoverage{
          .covered_rules = {},
          .non_covered_rule_codes = {1, 2, 3},
      }));

  EXPECT_EQ(
      RulesCoverage::create(
          rules,
          /* used_sources */ {source1, source2},
          /* used_sinks */ {partial_sink_a, partial_sink_b},
          /* used_transforms */ {}),
      (RulesCoverage{
          .covered_rules =
              {{3,
                CoveredRule{
                    .code = 3,
                    .used_sources = {source1, source2},
                    .used_sinks = {partial_sink_a, partial_sink_b},
                    .used_transforms = {}}}},
          .non_covered_rule_codes = {1, 2},
      }));
}

} // namespace marianatrench

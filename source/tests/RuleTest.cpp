/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>

#include <gmock/gmock.h>

#include <mariana-trench/MultiSourceMultiSinkRule.h>
#include <mariana-trench/SourceSinkRule.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

namespace {

class RuleTest : public test::Test {};

template <class RuleType>
std::vector<int> to_codes(const std::vector<const RuleType*>& rules) {
  std::vector<int> codes;
  for (const auto* rule : rules) {
    codes.push_back(rule->code());
  }
  return codes;
}

} // namespace

TEST_F(RuleTest, Rules) {
  auto context = test::make_empty_context();
  auto* source_a = context.kinds->get("A");
  auto* source_b = context.kinds->get("B");
  auto* sink_x = context.kinds->get("X");
  auto* sink_y = context.kinds->get("Y");
  auto* sink_z = context.kinds->get("Z");

  /* This creates the rule with the right combination of partial sinks.
     The testing of rule creation in practice is covered in JsonTest.cpp and
     asserted in the rule constructor. */
  auto* partial_sink_lbl_a = context.kinds->get_partial("kind", "labelA");
  auto* partial_sink_lbl_b = context.kinds->get_partial("kind", "labelB");

  std::vector<std::unique_ptr<Rule>> rule_list;
  rule_list.push_back(std::make_unique<SourceSinkRule>(
      /* name */ "Rule1",
      /* code */ 1,
      /* description */ "Test rule 1",
      /* source_kinds */ Rule::KindSet{source_a},
      /* sink_kinds */ Rule::KindSet{sink_x}));
  rule_list.push_back(std::make_unique<SourceSinkRule>(
      /* name */ "Rule2",
      /* code */ 2,
      /* description */ "Test rule 2",
      /* source_kinds */ Rule::KindSet{source_a},
      /* sink_kinds */ Rule::KindSet{sink_y}));
  rule_list.push_back(std::make_unique<SourceSinkRule>(
      /* name */ "Rule3",
      /* code */ 3,
      /* description */ "Test rule 3",
      /* source_kinds */ Rule::KindSet{source_b},
      /* sink_kinds */ Rule::KindSet{sink_y}));
  rule_list.push_back(std::make_unique<SourceSinkRule>(
      /* name */ "Rule4",
      /* code */ 4,
      /* description */ "Test rule 4",
      /* source_kinds */ Rule::KindSet{source_b},
      /* sink_kinds */ Rule::KindSet{sink_x, sink_y}));

  auto multi_source_rule = std::make_unique<MultiSourceMultiSinkRule>(
      /* name */ "Rule5",
      /* code */ 5,
      /* description */ "Test rule 5",
      /* multi_source_kinds */
      MultiSourceMultiSinkRule::MultiSourceKindsByLabel{
          {"labelA", {source_a, source_b}}, {"labelB", {source_a}}},
      /* partial_sink_kinds */
      MultiSourceMultiSinkRule::PartialKindSet{
          partial_sink_lbl_a, partial_sink_lbl_b});
  auto* triggered_sink_lbl_a =
      context.kinds->get_triggered(partial_sink_lbl_a, multi_source_rule.get());
  auto* triggered_sink_lbl_b =
      context.kinds->get_triggered(partial_sink_lbl_b, multi_source_rule.get());
  rule_list.push_back(std::move(multi_source_rule));

  auto rules = Rules(context, std::move(rule_list));

  /* Tests for matching of regular (SourceSink) rules */
  EXPECT_EQ(rules.size(), 5);
  EXPECT_THAT(
      to_codes(rules.rules(source_a, sink_x)),
      testing::UnorderedElementsAre(1));
  EXPECT_THAT(
      to_codes(rules.rules(source_a, sink_y)),
      testing::UnorderedElementsAre(2));
  EXPECT_TRUE(rules.rules(source_a, sink_z).empty());
  EXPECT_THAT(
      to_codes(rules.rules(source_b, sink_x)),
      testing::UnorderedElementsAre(4));
  EXPECT_THAT(
      to_codes(rules.rules(source_b, sink_y)),
      testing::UnorderedElementsAre(3, 4));
  EXPECT_TRUE(rules.rules(source_b, sink_z).empty());

  /* Tests for matching of MultiSourceMultiSink rules. */
  EXPECT_THAT(
      to_codes(rules.rules(source_a, triggered_sink_lbl_a)),
      testing::UnorderedElementsAre(5));
  EXPECT_THAT(
      to_codes(rules.rules(source_b, triggered_sink_lbl_a)),
      testing::UnorderedElementsAre(5));
  EXPECT_THAT(
      to_codes(rules.rules(source_a, triggered_sink_lbl_b)),
      testing::UnorderedElementsAre(5));
  EXPECT_TRUE(rules.rules(source_a, partial_sink_lbl_a).empty());
  EXPECT_TRUE(rules.rules(source_a, partial_sink_lbl_b).empty());
  EXPECT_TRUE(rules.rules(source_b, partial_sink_lbl_a).empty());
  EXPECT_TRUE(rules.rules(source_b, partial_sink_lbl_b).empty());
  EXPECT_TRUE(rules.rules(source_b, triggered_sink_lbl_b).empty());

  auto multi_source_rule_unused = std::make_unique<MultiSourceMultiSinkRule>(
      /* name */ "Rule6",
      /* code */ 6,
      /* description */ "Test rule 6",
      /* multi_source_kinds */
      MultiSourceMultiSinkRule::MultiSourceKindsByLabel{
          {"labelA", {source_a, source_b}}, {"labelB", {source_a}}},
      /* partial_sink_kinds */
      MultiSourceMultiSinkRule::PartialKindSet{
          partial_sink_lbl_a, partial_sink_lbl_b});
  auto* triggered_sink_invalid = context.kinds->get_triggered(
      partial_sink_lbl_a, multi_source_rule_unused.get());
  EXPECT_TRUE(rules.rules(source_a, triggered_sink_invalid).empty());

  EXPECT_THAT(
      to_codes(rules.partial_rules(source_a, partial_sink_lbl_a)),
      testing::UnorderedElementsAre(5));
  EXPECT_THAT(
      to_codes(rules.partial_rules(source_b, partial_sink_lbl_a)),
      testing::UnorderedElementsAre(5));
  EXPECT_THAT(
      to_codes(rules.partial_rules(source_a, partial_sink_lbl_b)),
      testing::UnorderedElementsAre(5));
  EXPECT_TRUE(rules.partial_rules(source_b, partial_sink_lbl_b).empty());
}

TEST_F(RuleTest, Uses) {
  auto context = test::make_empty_context();
  auto* source_a = context.kinds->get("A");
  auto* source_b = context.kinds->get("B");
  auto* sink_x = context.kinds->get("X");
  auto* sink_y = context.kinds->get("Y");

  /* Tests for SourceSinkRule */
  auto rule1 = std::make_unique<SourceSinkRule>(
      /* name */ "Rule1",
      /* code */ 1,
      /* description */ "Test rule 1",
      /* source_kinds */ Rule::KindSet{source_a},
      /* sink_kinds */ Rule::KindSet{sink_x});
  EXPECT_TRUE(rule1->uses(source_a));
  EXPECT_FALSE(rule1->uses(source_b));
  EXPECT_TRUE(rule1->uses(sink_x));
  EXPECT_FALSE(rule1->uses(sink_y));

  /* Tests for MultiSourceMultiSink rule */
  auto* partial_sink_lbl_a = context.kinds->get_partial("kind", "labelA");
  auto* partial_sink_lbl_b = context.kinds->get_partial("kind", "labelB");
  auto rule2 = std::make_unique<MultiSourceMultiSinkRule>(
      /* name */ "Rule2",
      /* code */ 2,
      /* description */ "Test rule 2",
      /* multi_source_kinds */
      MultiSourceMultiSinkRule::MultiSourceKindsByLabel{
          {"labelA", {source_a}}, {"labelB", {source_b}}},
      /* partial_sink_kinds */
      MultiSourceMultiSinkRule::PartialKindSet{
          partial_sink_lbl_a, partial_sink_lbl_b});
  EXPECT_TRUE(rule2->uses(source_a));
  EXPECT_TRUE(rule2->uses(source_b));
  EXPECT_TRUE(rule2->uses(partial_sink_lbl_a));
  EXPECT_TRUE(rule2->uses(partial_sink_lbl_b));

  auto* triggered_sink_lbl_a =
      context.kinds->get_triggered(partial_sink_lbl_a, rule2.get());
  auto* triggered_sink_lbl_b =
      context.kinds->get_triggered(partial_sink_lbl_b, rule2.get());
  EXPECT_FALSE(rule2->uses(triggered_sink_lbl_a));
  EXPECT_FALSE(rule2->uses(triggered_sink_lbl_b));
  EXPECT_FALSE(rule2->uses(sink_y));
}

} // namespace marianatrench

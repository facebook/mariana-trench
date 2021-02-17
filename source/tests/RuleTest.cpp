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

std::vector<int> to_codes(const std::vector<const Rule*>& rules) {
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
  rule_list.push_back(std::make_unique<MultiSourceMultiSinkRule>(
      /* name */ "Rule5",
      /* code */ 5,
      /* description */ "Test rule 5",
      /* multi_source_kinds */
      MultiSourceMultiSinkRule::MultiSourceKindsByLabel{
          {"labelA", {source_a, source_b}}, {"labelB", {source_a}}},
      /* partial_sink_kinds */ Rule::KindSet{sink_x}));

  auto rules = Rules(std::move(rule_list));

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
}

TEST_F(RuleTest, Uses) {
  auto context = test::make_empty_context();
  auto* source_a = context.kinds->get("A");
  auto* source_b = context.kinds->get("B");
  auto* sink_x = context.kinds->get("X");
  auto* sink_y = context.kinds->get("Y");

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

  // TBD: Check for MultiSourceMultiSink rule as well.
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>

#include <gmock/gmock.h>

#include <mariana-trench/MultiCaseRule.h>
#include <mariana-trench/MultiSourceMultiSinkRule.h>
#include <mariana-trench/SourceSinkRule.h>
#include <mariana-trench/SourceSinkWithExploitabilityRule.h>
#include <mariana-trench/TransformList.h>
#include <mariana-trench/TransformsFactory.h>
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
  auto* source_a = context.kind_factory->get("A");
  auto* source_b = context.kind_factory->get("B");
  auto* sink_x = context.kind_factory->get("X");
  auto* sink_y = context.kind_factory->get("Y");
  auto* sink_z = context.kind_factory->get("Z");

  /* This creates the rule with the right combination of partial sinks.
     The testing of rule creation in practice is covered in JsonTest.cpp and
     asserted in the rule constructor. */
  auto* partial_sink_lbl_a =
      context.kind_factory->get_partial("kind", "labelA");
  auto* partial_sink_lbl_b =
      context.kind_factory->get_partial("kind", "labelB");

  std::vector<std::unique_ptr<Rule>> rule_list;
  rule_list.push_back(
      std::make_unique<SourceSinkRule>(
          /* name */ "Rule1",
          /* code */ 1,
          /* description */ "Test rule 1",
          /* source_kinds */ Rule::KindSet{source_a},
          /* sink_kinds */ Rule::KindSet{sink_x},
          /* transforms */ nullptr));
  rule_list.push_back(
      std::make_unique<SourceSinkRule>(
          /* name */ "Rule2",
          /* code */ 2,
          /* description */ "Test rule 2",
          /* source_kinds */ Rule::KindSet{source_a},
          /* sink_kinds */ Rule::KindSet{sink_y},
          /* transforms */ nullptr));
  rule_list.push_back(
      std::make_unique<SourceSinkRule>(
          /* name */ "Rule3",
          /* code */ 3,
          /* description */ "Test rule 3",
          /* source_kinds */ Rule::KindSet{source_b},
          /* sink_kinds */ Rule::KindSet{sink_y},
          /* transforms */ nullptr));
  rule_list.push_back(
      std::make_unique<SourceSinkRule>(
          /* name */ "Rule4",
          /* code */ 4,
          /* description */ "Test rule 4",
          /* source_kinds */ Rule::KindSet{source_b},
          /* sink_kinds */ Rule::KindSet{sink_x, sink_y},
          /* transforms */ nullptr));

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
  auto* triggered_sink_lbl_a = context.kind_factory->get_triggered(
      partial_sink_lbl_a, multi_source_rule->code());
  auto* triggered_sink_lbl_b = context.kind_factory->get_triggered(
      partial_sink_lbl_b, multi_source_rule->code());
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
  auto* triggered_sink_invalid = context.kind_factory->get_triggered(
      partial_sink_lbl_a, multi_source_rule_unused->code());
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

TEST_F(RuleTest, TransformRules) {
  auto context = test::make_empty_context();
  auto* source_a = context.kind_factory->get("A");
  auto* source_b = context.kind_factory->get("B");
  auto* sink_x = context.kind_factory->get("X");
  auto* sink_y = context.kind_factory->get("Y");

  auto* t1 = context.transforms_factory->create({"T1"}, context);
  auto* t2 = context.transforms_factory->create({"T2"}, context);
  auto* t12 = context.transforms_factory->create({"T1", "T2"}, context);
  auto* t21 = context.transforms_factory->create({"T2", "T1"}, context);

  EXPECT_EQ(t1->size(), 1);
  EXPECT_EQ(t2->size(), 1);
  EXPECT_EQ(t12->size(), 2);
  EXPECT_EQ(t21->size(), 2);

  std::vector<std::unique_ptr<Rule>> rule_list;
  rule_list.push_back(
      std::make_unique<SourceSinkRule>(
          /* name */ "Rule1",
          /* code */ 1,
          /* description */ "Test rule 1",
          /* source_kinds */ Rule::KindSet{source_a},
          /* sink_kinds */ Rule::KindSet{sink_x},
          /* transforms */ t1));
  rule_list.push_back(
      std::make_unique<SourceSinkRule>(
          /* name */ "Rule2",
          /* code */ 2,
          /* description */ "Test rule 2",
          /* source_kinds */ Rule::KindSet{source_a},
          /* sink_kinds */ Rule::KindSet{sink_x},
          /* transforms */ t12));
  rule_list.push_back(
      std::make_unique<SourceSinkRule>(
          /* name */ "Rule3",
          /* code */ 3,
          /* description */ "Test rule 3",
          /* source_kinds */ Rule::KindSet{source_b},
          /* sink_kinds */ Rule::KindSet{sink_y},
          /* transforms */ t12));

  auto rules = Rules(context, std::move(rule_list));

  EXPECT_EQ(rules.size(), 3);

  // Rule 1 checks
  EXPECT_THAT(
      to_codes(rules.rules(
          context.kind_factory->transform_kind(
              /* base_kind */ source_a,
              /* local_transforms */ t1,
              /* global_transforms */ nullptr),
          sink_x)),
      testing::UnorderedElementsAre(1));
  EXPECT_THAT(
      to_codes(rules.rules(
          context.kind_factory->transform_kind(
              /* base_kind */ source_a,
              /* local_transforms */ nullptr,
              /* global_transforms */ t1),
          sink_x)),
      testing::UnorderedElementsAre(1));
  EXPECT_TRUE(to_codes(rules.rules(
                           context.kind_factory->transform_kind(
                               /* base_kind */ source_a,
                               /* local_transforms */ t1,
                               /* global_transforms */ t1),
                           sink_x))
                  .empty());
  EXPECT_TRUE(to_codes(rules.rules(
                           context.kind_factory->transform_kind(
                               /* base_kind */ source_a,
                               /* local_transforms */ t2,
                               /* global_transforms */ nullptr),
                           sink_x))
                  .empty());
  EXPECT_THAT(
      to_codes(rules.rules(
          source_a,
          context.kind_factory->transform_kind(
              /* base_kind */ sink_x,
              /* local_transforms */ t1,
              /* global_transforms */ nullptr))),
      testing::UnorderedElementsAre(1));
  EXPECT_THAT(
      to_codes(rules.rules(
          source_a,
          context.kind_factory->transform_kind(
              /* base_kind */ sink_x,
              /* local_transforms */ nullptr,
              /* global_transforms */ t1))),
      testing::UnorderedElementsAre(1));
  EXPECT_TRUE(to_codes(rules.rules(
                           source_a,
                           context.kind_factory->transform_kind(
                               /* base_kind */ sink_x,
                               /* local_transforms */ t1,
                               /* global_transforms */ t1)))
                  .empty());

  // Rule 2 checks
  EXPECT_THAT(
      to_codes(rules.rules(
          context.kind_factory->transform_kind(
              /* base_kind */ source_a,
              /* local_transforms */ t21,
              /* global_transforms */ nullptr),
          sink_x)),
      testing::UnorderedElementsAre(2));
  EXPECT_THAT(
      to_codes(rules.rules(
          context.kind_factory->transform_kind(
              /* base_kind */ source_a,
              /* local_transforms */ nullptr,
              /* global_transforms */ t21),
          sink_x)),
      testing::UnorderedElementsAre(2));
  EXPECT_THAT(
      to_codes(rules.rules(
          context.kind_factory->transform_kind(
              /* base_kind */ source_a,
              /* local_transforms */ t2,
              /* global_transforms */ t1),
          sink_x)),
      testing::UnorderedElementsAre(2));

  EXPECT_TRUE(to_codes(rules.rules(
                           context.kind_factory->transform_kind(
                               /* base_kind */ source_a,
                               /* local_transforms */ t12,
                               /* global_transforms */ nullptr),
                           sink_x))
                  .empty());
  EXPECT_TRUE(to_codes(rules.rules(
                           context.kind_factory->transform_kind(
                               /* base_kind */ source_a,
                               /* local_transforms */ nullptr,
                               /* global_transforms */ t12),
                           sink_x))
                  .empty());

  EXPECT_THAT(
      to_codes(rules.rules(
          context.kind_factory->transform_kind(
              /* base_kind */ source_a,
              /* local_transforms */ t1,
              /* global_transforms */ nullptr),
          context.kind_factory->transform_kind(
              /* base_kind */ sink_x,
              /* local_transforms */ t2,
              /* global_transforms */ nullptr))),
      testing::UnorderedElementsAre(2));

  EXPECT_THAT(
      to_codes(rules.rules(
          source_a,
          context.kind_factory->transform_kind(
              /* base_kind */ sink_x,
              /* local_transforms */ t12,
              /* global_transforms */ nullptr))),
      testing::UnorderedElementsAre(2));
  EXPECT_THAT(
      to_codes(rules.rules(
          source_a,
          context.kind_factory->transform_kind(
              /* base_kind */ sink_x,
              /* local_transforms */ nullptr,
              /* global_transforms */ t12))),
      testing::UnorderedElementsAre(2));
  EXPECT_THAT(
      to_codes(rules.rules(
          source_a,
          context.kind_factory->transform_kind(
              /* base_kind */ sink_x,
              /* local_transforms */ t1,
              /* global_transforms */ t2))),
      testing::UnorderedElementsAre(2));
  EXPECT_TRUE(to_codes(rules.rules(
                           source_a,
                           context.kind_factory->transform_kind(
                               /* base_kind */ sink_x,
                               /* local_transforms */ t1,
                               /* global_transforms */ t1)))
                  .empty());
  // Rule 3 checks
  EXPECT_THAT(
      to_codes(rules.rules(
          context.kind_factory->transform_kind(
              /* base_kind */ source_b,
              /* local_transforms */ t1,
              /* global_transforms */ nullptr),
          context.kind_factory->transform_kind(
              /* base_kind */ sink_y,
              /* local_transforms */ t2,
              /* global_transforms */ nullptr))),
      testing::UnorderedElementsAre(3));
}

TEST_F(RuleTest, Uses) {
  auto context = test::make_empty_context();
  auto* source_a = context.kind_factory->get("A");
  auto* source_b = context.kind_factory->get("B");
  auto* sink_x = context.kind_factory->get("X");
  auto* sink_y = context.kind_factory->get("Y");

  /* Tests for SourceSinkRule */
  auto rule1 = std::make_unique<SourceSinkRule>(
      /* name */ "Rule1",
      /* code */ 1,
      /* description */ "Test rule 1",
      /* source_kinds */ Rule::KindSet{source_a},
      /* sink_kinds */ Rule::KindSet{sink_x},
      /* transforms */ nullptr);
  EXPECT_TRUE(rule1->uses(source_a));
  EXPECT_FALSE(rule1->uses(source_b));
  EXPECT_TRUE(rule1->uses(sink_x));
  EXPECT_FALSE(rule1->uses(sink_y));

  /* Tests for MultiSourceMultiSink rule */
  auto* partial_sink_lbl_a =
      context.kind_factory->get_partial("kind", "labelA");
  auto* partial_sink_lbl_b =
      context.kind_factory->get_partial("kind", "labelB");
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
      context.kind_factory->get_triggered(partial_sink_lbl_a, rule2->code());
  auto* triggered_sink_lbl_b =
      context.kind_factory->get_triggered(partial_sink_lbl_b, rule2->code());
  EXPECT_FALSE(rule2->uses(triggered_sink_lbl_a));
  EXPECT_FALSE(rule2->uses(triggered_sink_lbl_b));
  EXPECT_FALSE(rule2->uses(sink_y));
}

TEST_F(RuleTest, SourceSinkWithExploitabilityRuleTest) {
  auto context = test::make_empty_context();
  auto* source_a = context.kind_factory->get("A");
  auto* source_b = context.kind_factory->get("B");
  auto* source_a_as_transform = context.transforms_factory->create(
      TransformList::from_kind(source_a, context));
  auto* source_b_as_transform = context.transforms_factory->create(
      TransformList::from_kind(source_b, context));
  auto* effect_source_e = context.kind_factory->get("E");
  auto* sink_x = context.kind_factory->get("X");
  auto* sink_y = context.kind_factory->get("Y");

  auto rule1 = std::make_unique<SourceSinkWithExploitabilityRule>(
      /* name */ "Rule1",
      /* code */ 1,
      /* description */ "Test rule 1",
      /* effect_source_kinds */ Rule::KindSet{effect_source_e},
      /* source_kinds */ Rule::KindSet{source_a},
      /* sink_kinds */ Rule::KindSet{sink_x},
      /* source_as_transforms */
      SourceSinkWithExploitabilityRule::KindToTransformsMap{
          {source_a, source_a_as_transform}});
  EXPECT_TRUE(rule1->uses(effect_source_e));
  EXPECT_TRUE(rule1->uses(source_a));
  EXPECT_FALSE(rule1->uses(source_b));
  EXPECT_TRUE(rule1->uses(sink_x));
  EXPECT_FALSE(rule1->uses(sink_y));

  auto rule2 = std::make_unique<SourceSinkWithExploitabilityRule>(
      /* name */ "Rule2",
      /* code */ 2,
      /* description */ "Test rule 2",
      /* effect_source_kinds */ Rule::KindSet{effect_source_e},
      /* source_kinds */ Rule::KindSet{source_a, source_b},
      /* sink_kinds */
      Rule::KindSet{sink_x, sink_y},
      /* source_as_transforms */
      SourceSinkWithExploitabilityRule::KindToTransformsMap{
          {source_a, source_a_as_transform},
          {source_b, source_b_as_transform}});
  EXPECT_TRUE(rule2->uses(effect_source_e));
  EXPECT_TRUE(rule2->uses(source_a));
  EXPECT_TRUE(rule2->uses(source_b));
  EXPECT_TRUE(rule2->uses(sink_x));
  EXPECT_TRUE(rule2->uses(sink_y));

  // TODO: T176363060 Add tests for checking rule matches.
}

TEST_F(RuleTest, SerializationDeserialization) {
  auto context = test::make_empty_context();
  auto* source_a = context.kind_factory->get("A");
  auto* source_b = context.kind_factory->get("B");

  /* This creates the rule with the right combination of partial sinks.
     The testing of rule creation in practice is covered in JsonTest.cpp and
     asserted in the rule constructor. */
  auto* partial_sink_lbl_a =
      context.kind_factory->get_partial("kind", "labelA");
  auto* partial_sink_lbl_b =
      context.kind_factory->get_partial("kind", "labelB");

  auto multi_source_rule = MultiSourceMultiSinkRule(
      /* name */ "Rule1",
      /* code */ 1,
      /* description */ "Test rule 1",
      /* multi_source_kinds */
      MultiSourceMultiSinkRule::MultiSourceKindsByLabel{
          {"labelA", {source_a, source_b}}, {"labelB", {source_a}}},
      /* partial_sink_kinds */
      MultiSourceMultiSinkRule::PartialKindSet{
          partial_sink_lbl_a, partial_sink_lbl_b});
  auto rule_ptr = Rule::from_json(
      multi_source_rule.to_json(/* include_metadata */ true), context);
  EXPECT_EQ(
      *dynamic_cast<const MultiSourceMultiSinkRule*>(rule_ptr.get()),
      multi_source_rule);
}

TEST_F(RuleTest, MultiCaseRuleBasic) {
  auto context = test::make_empty_context();
  auto* source_a = context.kind_factory->get("A");
  auto* source_b = context.kind_factory->get("B");
  auto* sink_x = context.kind_factory->get("X");
  auto* sink_y = context.kind_factory->get("Y");
  auto* sink_z = context.kind_factory->get("Z");

  auto name = "Multicase Rule";
  auto code = 1;
  auto description = "Test multi-case rule";

  std::vector<std::unique_ptr<Rule>> cases;
  cases.push_back(
      std::make_unique<SourceSinkRule>(
          /* name */ name,
          /* code */ code,
          /* description */ description,
          /* source_kinds */ Rule::KindSet{source_a},
          /* sink_kinds */ Rule::KindSet{sink_x, sink_y},
          /* transforms */ nullptr));
  cases.push_back(
      std::make_unique<SourceSinkRule>(
          /* name */ name,
          /* code */ code,
          /* description */ description,
          /* source_kinds */ Rule::KindSet{source_b},
          /* sink_kinds */ Rule::KindSet{sink_z},
          /* transforms */ nullptr));

  std::vector<std::unique_ptr<Rule>> rule_list;
  rule_list.push_back(
      std::make_unique<MultiCaseRule>(
          /* name */ name,
          /* code */ code,
          /* description */ description,
          /* cases */ std::move(cases)));

  auto rules = Rules(context, std::move(rule_list));

  EXPECT_EQ(rules.size(), 1);
  // Case 1: source A -> sink X
  EXPECT_THAT(
      to_codes(rules.rules(source_a, sink_x)),
      testing::UnorderedElementsAre(1));
  // Case 1: source A -> sink Y
  EXPECT_THAT(
      to_codes(rules.rules(source_a, sink_y)),
      testing::UnorderedElementsAre(1));
  // Case 2: source B -> sink Z
  EXPECT_THAT(
      to_codes(rules.rules(source_b, sink_z)),
      testing::UnorderedElementsAre(1));
  // No match: source A -> sink Z
  EXPECT_TRUE(rules.rules(source_a, sink_z).empty());
  // No match: source B -> sink X
  EXPECT_TRUE(rules.rules(source_b, sink_x).empty());
}

TEST_F(RuleTest, MultiCaseRuleWithTransforms) {
  auto context = test::make_empty_context();
  auto* source_a = context.kind_factory->get("A");
  auto* source_b = context.kind_factory->get("B");
  auto* sink_x = context.kind_factory->get("X");
  auto* sink_y = context.kind_factory->get("Y");

  auto* t1 = context.transforms_factory->create({"T1"}, context);
  auto* t2 = context.transforms_factory->create({"T2"}, context);

  auto name = "Multicase Rule";
  auto code = 1;
  auto description = "Test multi-case rule with transforms";

  std::vector<std::unique_ptr<Rule>> cases;
  cases.push_back(
      std::make_unique<SourceSinkRule>(
          /* name */ name,
          /* code */ code,
          /* description */ description,
          /* source_kinds */ Rule::KindSet{source_a},
          /* sink_kinds */ Rule::KindSet{sink_x},
          /* transforms */ t1));
  cases.push_back(
      std::make_unique<SourceSinkRule>(
          /* name */ name,
          /* code */ code,
          /* description */ description,
          /* source_kinds */ Rule::KindSet{source_b},
          /* sink_kinds */ Rule::KindSet{sink_y},
          /* transforms */ t2));

  std::vector<std::unique_ptr<Rule>> rule_list;
  rule_list.push_back(
      std::make_unique<MultiCaseRule>(
          /* name */ name,
          /* code */ code,
          /* description */ description,
          /* cases */ std::move(cases)));

  auto rules = Rules(context, std::move(rule_list));

  EXPECT_EQ(rules.size(), 1);

  // Case 1: source A with T1 transform -> sink X
  EXPECT_THAT(
      to_codes(rules.rules(
          context.kind_factory->transform_kind(
              /* base_kind */ source_a,
              /* local_transforms */ t1,
              /* global_transforms */ nullptr),
          sink_x)),
      testing::UnorderedElementsAre(1));

  // Case 2: source B with T2 transform -> sink Y
  EXPECT_THAT(
      to_codes(rules.rules(
          context.kind_factory->transform_kind(
              /* base_kind */ source_b,
              /* local_transforms */ t2,
              /* global_transforms */ nullptr),
          sink_y)),
      testing::UnorderedElementsAre(1));

  // No match: source A without transform -> sink X
  EXPECT_TRUE(rules.rules(source_a, sink_x).empty());

  // No match: source A with wrong transform -> sink X
  EXPECT_TRUE(to_codes(rules.rules(
                           context.kind_factory->transform_kind(
                               /* base_kind */ source_a,
                               /* local_transforms */ t2,
                               /* global_transforms */ nullptr),
                           sink_x))
                  .empty());
}

TEST_F(RuleTest, MultiCaseRuleUses) {
  auto context = test::make_empty_context();
  auto* source_a = context.kind_factory->get("A");
  auto* source_b = context.kind_factory->get("B");
  auto* source_c = context.kind_factory->get("C");
  auto* sink_x = context.kind_factory->get("X");
  auto* sink_y = context.kind_factory->get("Y");
  auto* sink_z = context.kind_factory->get("Z");

  auto* effect_source_e = context.kind_factory->get("E");
  auto* source_a_as_transform = context.transforms_factory->create(
      TransformList::from_kind(source_a, context));

  auto name = "Multicase Rule";
  auto code = 1;
  auto description =
      "Test multi-case rule with transforms and exploitability rule";

  std::vector<std::unique_ptr<Rule>> cases;
  cases.push_back(
      std::make_unique<SourceSinkRule>(
          /* name */ name,
          /* code */ code,
          /* description */ description,
          /* source_kinds */ Rule::KindSet{source_a},
          /* sink_kinds */ Rule::KindSet{sink_x},
          /* transforms */ nullptr));
  cases.push_back(
      std::make_unique<SourceSinkRule>(
          /* name */ name,
          /* code */ code,
          /* description */ description,
          /* source_kinds */ Rule::KindSet{source_b},
          /* sink_kinds */ Rule::KindSet{sink_y},
          /* transforms */ nullptr));

  cases.push_back(
      std::make_unique<SourceSinkWithExploitabilityRule>(
          /* name */ name,
          /* code */ code,
          /* description */ description,
          /* effect_source_kinds */ Rule::KindSet{effect_source_e},
          /* source_kinds */ Rule::KindSet{source_c},
          /* sink_kinds */
          Rule::KindSet{sink_z},
          /* source_as_transforms */
          SourceSinkWithExploitabilityRule::KindToTransformsMap{
              {source_a, source_a_as_transform},
          }));

  auto rule = std::make_unique<MultiCaseRule>(
      /* name */ name,
      /* code */ code,
      /* description */ description,
      /* cases */ std::move(cases));

  EXPECT_TRUE(rule->uses(source_a));
  EXPECT_TRUE(rule->uses(source_b));
  EXPECT_TRUE(rule->uses(sink_x));
  EXPECT_TRUE(rule->uses(sink_y));

  // Exploitability rule
  EXPECT_TRUE(rule->uses(effect_source_e));
  EXPECT_TRUE(rule->uses(source_c));
  EXPECT_TRUE(rule->uses(sink_z));
}

TEST_F(RuleTest, MultiCaseRuleJsonSerialization) {
  auto context = test::make_empty_context();
  auto* source_a = context.kind_factory->get("A");
  auto* source_b = context.kind_factory->get("B");
  auto* source_c = context.kind_factory->get("C");
  auto* sink_x = context.kind_factory->get("X");
  auto* sink_y = context.kind_factory->get("Y");
  auto* sink_z = context.kind_factory->get("Z");

  auto* t1 = context.transforms_factory->create({"T1"}, context);

  auto* effect_source_e = context.kind_factory->get("E");
  auto* source_a_as_transform = context.transforms_factory->create(
      TransformList::from_kind(source_a, context));

  auto name = "Multicase Rule";
  auto code = 1;
  auto description =
      "Test multi-case rule with transforms and exploitability rule";

  std::vector<std::unique_ptr<Rule>> cases;
  cases.push_back(
      std::make_unique<SourceSinkRule>(
          /* name */ name,
          /* code */ code,
          /* description */ description,
          /* source_kinds */ Rule::KindSet{source_a},
          /* sink_kinds */ Rule::KindSet{sink_x},
          /* transforms */ nullptr));
  cases.push_back(
      std::make_unique<SourceSinkRule>(
          /* name */ name,
          /* code */ code,
          /* description */ description,
          /* source_kinds */ Rule::KindSet{source_b},
          /* sink_kinds */ Rule::KindSet{sink_y},
          /* transforms */ t1));

  cases.push_back(
      std::make_unique<SourceSinkWithExploitabilityRule>(
          /* name */ name,
          /* code */ code,
          /* description */ description,
          /* effect_source_kinds */ Rule::KindSet{effect_source_e},
          /* source_kinds */ Rule::KindSet{source_c},
          /* sink_kinds */
          Rule::KindSet{sink_z},
          /* source_as_transforms */
          SourceSinkWithExploitabilityRule::KindToTransformsMap{
              {source_a, source_a_as_transform},
          }));

  auto multi_case_rule = MultiCaseRule(
      /* name */ name,
      /* code */ code,
      /* description */ description,
      /* cases */ std::move(cases));

  auto json = multi_case_rule.to_json(/* include_metadata */ true);
  EXPECT_EQ(json["name"].asString(), name);
  EXPECT_EQ(json["code"].asInt(), code);
  EXPECT_EQ(json["description"].asString(), description);
  EXPECT_TRUE(json.isMember("cases"));
  EXPECT_EQ(json["cases"].size(), 3);

  // Verify first case
  EXPECT_TRUE(json["cases"][0].isMember("sources"));
  EXPECT_TRUE(json["cases"][0].isMember("sinks"));

  // Verify second case has transforms
  EXPECT_TRUE(json["cases"][1].isMember("transforms"));

  // Verify third case has effect source
  EXPECT_TRUE(json["cases"][2].isMember("effect_sources"));
}

} // namespace marianatrench

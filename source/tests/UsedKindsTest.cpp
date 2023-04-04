/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>

#include <gmock/gmock.h>

#include <mariana-trench/SourceSinkRule.h>
#include <mariana-trench/TransformList.h>
#include <mariana-trench/TransformsFactory.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

namespace {

class UsedKindsTest : public test::Test {};

std::unordered_set<std::string> to_trace_strings(
    std::unordered_set<
        std::pair<const TransformList*, const TransformList*>,
        boost::hash<std::pair<const TransformList*, const TransformList*>>>&
        partitions) {
  std::unordered_set<std::string> all_transforms{};

  for (const auto& [left, right] : partitions) {
    all_transforms.insert(left->to_trace_string());
    all_transforms.insert(right->to_trace_string());
  }

  return all_transforms;
}

std::unordered_set<std::string> to_trace_strings(
    const std::unordered_set<const TransformList*>& transforms) {
  std::unordered_set<std::string> all_transforms{};

  for (const auto* transform : transforms) {
    all_transforms.insert(transform->to_trace_string());
  }

  return all_transforms;
}

} // namespace

TEST_F(UsedKindsTest, Combinations) {
  auto context = test::make_empty_context();
  auto* t1234 =
      context.transforms_factory->create({"1", "2", "3", "4"}, context);

  auto combinations = context.transforms_factory->all_combinations(t1234);
  EXPECT_EQ(combinations.transform->to_trace_string(), "1:2:3:4");

  EXPECT_THAT(
      to_trace_strings(combinations.partitions),
      testing::UnorderedElementsAre("1", "2:3:4", "1:2", "3:4", "1:2:3", "4"));

  EXPECT_THAT(
      to_trace_strings(combinations.subsequences),
      testing::UnorderedElementsAre("2", "2:3", "3"));
}

TEST_F(UsedKindsTest, UsedTransformKinds) {
  auto context = test::make_empty_context();
  const auto* source_a = context.kind_factory->get("A");
  const auto* source_b = context.kind_factory->get("B");
  const auto* sink_x = context.kind_factory->get("X");
  const auto* sink_y = context.kind_factory->get("Y");

  const auto* t11 = context.transforms_factory->create({"1", "1"}, context);
  const auto* t1234 =
      context.transforms_factory->create({"1", "2", "3", "4"}, context);

  EXPECT_EQ(t11->size(), 2);
  EXPECT_EQ(t1234->size(), 4);

  std::vector<std::unique_ptr<Rule>> rule_list;
  rule_list.push_back(std::make_unique<SourceSinkRule>(
      /* name */ "Rule1",
      /* code */ 1,
      /* description */ "Test rule 1",
      /* source_kinds */ Rule::KindSet{source_a},
      /* sink_kinds */ Rule::KindSet{sink_x},
      /* transforms */ t11));
  rule_list.push_back(std::make_unique<SourceSinkRule>(
      /* name */ "Rule2",
      /* code */ 2,
      /* description */ "Test rule 2",
      /* source_kinds */ Rule::KindSet{source_a, source_b},
      /* sink_kinds */ Rule::KindSet{sink_x, sink_y},
      /* transforms */ t1234));

  auto rules = Rules(context, std::move(rule_list));
  auto used_kinds = UsedKinds::from_rules(rules, *context.transforms_factory);

  const auto& named_kind_to_transforms = used_kinds.named_kind_to_transforms();
  const auto& propagation_kind_to_transforms =
      used_kinds.propagation_kind_to_transforms();

  EXPECT_THAT(
      to_trace_strings(named_kind_to_transforms.at(source_a)),
      testing::UnorderedElementsAre("1", "1:1", "2:1", "3:2:1", "4:3:2:1"));

  EXPECT_THAT(
      to_trace_strings(named_kind_to_transforms.at(sink_x)),
      testing::UnorderedElementsAre(
          "1:1", "1", "1:2:3:4", "2:3:4", "3:4", "4"));

  EXPECT_THAT(
      to_trace_strings(named_kind_to_transforms.at(source_b)),
      testing::UnorderedElementsAre("1", "2:1", "3:2:1", "4:3:2:1"));

  EXPECT_THAT(
      to_trace_strings(named_kind_to_transforms.at(sink_y)),
      testing::UnorderedElementsAre("1:2:3:4", "2:3:4", "3:4", "4"));

  EXPECT_THAT(
      to_trace_strings(propagation_kind_to_transforms),
      testing::UnorderedElementsAre(
          "1",
          "1:1",
          "1:2",
          "1:2:3",
          "1:2:3:4",
          "2",
          "2:3",
          "2:3:4",
          "3",
          "3:4",
          "4"));
}

} // namespace marianatrench

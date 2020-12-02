// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <gmock/gmock.h>

#include <mariana-trench/Rule.h>
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

  auto rules = Rules({
      Rule(
          /* name */ "Rule1",
          /* code */ 1,
          /* description */ "Test rule 1",
          /* source_kinds */ {source_a},
          /* sink_kinds */ {sink_x}),
      Rule(
          /* name */ "Rule2",
          /* code */ 2,
          /* description */ "Test rule 2",
          /* source_kinds */ {source_a},
          /* sink_kinds */ {sink_y}),
      Rule(
          /* name */ "Rule3",
          /* code */ 3,
          /* description */ "Test rule 3",
          /* source_kinds */ {source_b},
          /* sink_kinds */ {sink_y}),
      Rule(
          /* name */ "Rule4",
          /* code */ 4,
          /* description */ "Test rule 4",
          /* source_kinds */ {source_b},
          /* sink_kinds */ {sink_x, sink_y}),
  });

  EXPECT_EQ(rules.size(), 4);
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

} // namespace marianatrench

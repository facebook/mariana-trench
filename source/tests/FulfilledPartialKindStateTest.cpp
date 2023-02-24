/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/FulfilledPartialKindState.h>
#include <mariana-trench/MultiSourceMultiSinkRule.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class FulfilledPartialKindStateTest : public test::Test {};

TEST_F(FulfilledPartialKindStateTest, Basic) {
  auto scope = Scope();
  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  // Make method context.
  auto registry = Registry(context);
  auto* dex_method = redex::create_void_method(
      scope,
      /* class_name */ "MyClass;",
      /* method_name */ "<init>",
      /* parameter_types */ "I",
      /* return_type*/ "V",
      /* super */ nullptr);
  auto* method = context.methods->create(dex_method);
  auto model = Model(/* method */ method, context);

  auto state = FulfilledPartialKindState();

  const auto feature_1 = context.features->get("Feature1");
  const auto feature_2 = context.features->get("Feature2");
  const auto* source_1 = context.kinds->get("Source1");
  const auto* source_2 = context.kinds->get("Source2");
  const auto* fulfilled = context.kinds->get_partial("Partial", "a");
  const auto* unfulfilled = context.kinds->get_partial("Partial", "b'");

  auto rule_1 = std::make_unique<MultiSourceMultiSinkRule>(
      /* name */ "Rule1",
      /* code */ 1,
      /* description */ "rule 1",
      /* multi_source_kinds */
      MultiSourceMultiSinkRule::MultiSourceKindsByLabel{
          {"a", Rule::KindSet{source_1}}, {"b", Rule::KindSet{source_2}}},
      /* partial_sink_kinds */
      MultiSourceMultiSinkRule::PartialKindSet{fulfilled, unfulfilled});

  // Exactly the same rule, but different code.
  auto rule_2 = std::make_unique<MultiSourceMultiSinkRule>(
      /* name */ "Rule2",
      /* code */ 2,
      /* description */ "rule 2",
      /* multi_source_kinds */
      MultiSourceMultiSinkRule::MultiSourceKindsByLabel{
          {"a", Rule::KindSet{source_1}}, {"b", Rule::KindSet{source_2}}},
      /* partial_sink_kinds */
      MultiSourceMultiSinkRule::PartialKindSet{fulfilled, unfulfilled});

  auto sink_frame = test::make_taint_config(
      /* kind */ fulfilled, test::FrameProperties{});

  // Simulate kind fulfilled, i.e. source_1 -> fulfilled under rule_1 with
  // some features. Note: rule_2 not fulfilled.
  EXPECT_EQ(
      std::nullopt,
      state.fulfill_kind(
          /* kind */ fulfilled,
          /* rule */ rule_1.get(),
          /* features */ FeatureMayAlwaysSet{feature_1},
          /* sink */ Taint{sink_frame},
          *context.kinds));
  EXPECT_EQ(
      FeatureMayAlwaysSet{feature_1},
      state.get_features(fulfilled, rule_1.get()));
  EXPECT_EQ(
      fulfilled, state.get_fulfilled_counterpart(unfulfilled, rule_1.get()));
  EXPECT_EQ(
      nullptr, state.get_fulfilled_counterpart(unfulfilled, rule_2.get()));

  // Get triggered counterparts for the unfulfilled kind in rule_1.
  EXPECT_THAT(
      state.make_triggered_counterparts(unfulfilled, *context.kinds),
      testing::UnorderedElementsAre(
          context.kinds->get_triggered(unfulfilled, rule_1.get())));

  // Fulfill `fulfilled` under rule_2 as well.
  EXPECT_EQ(
      std::nullopt,
      state.fulfill_kind(
          /* kind */ fulfilled,
          /* rule */ rule_2.get(),
          /* features */ FeatureMayAlwaysSet{},
          /* sink */ Taint{sink_frame},
          *context.kinds));
  EXPECT_EQ(FeatureMayAlwaysSet{}, state.get_features(fulfilled, rule_2.get()));
  EXPECT_EQ(
      fulfilled, state.get_fulfilled_counterpart(unfulfilled, rule_2.get()));

  // Triggered counterparts now includes rule_2
  EXPECT_THAT(
      state.make_triggered_counterparts(unfulfilled, *context.kinds),
      testing::UnorderedElementsAre(
          context.kinds->get_triggered(unfulfilled, rule_1.get()),
          context.kinds->get_triggered(unfulfilled, rule_2.get())));

  // Fulfill the other part of rule_1.
  auto unfulfilled_sink_frame = test::make_taint_config(
      /* kind */ unfulfilled,
      test::FrameProperties{
          .inferred_features = FeatureMayAlwaysSet{feature_2}});
  EXPECT_EQ(
      state
          .fulfill_kind(
              /* kind */ unfulfilled,
              /* rule */ rule_1.get(),
              /* features */ FeatureMayAlwaysSet{},
              /* sink */ Taint{unfulfilled_sink_frame},
              *context.kinds)
          .value(),
      Taint{test::make_taint_config(
          /* kind */ context.kinds->get_triggered(unfulfilled, rule_1.get()),
          test::FrameProperties{
              .inferred_features = FeatureMayAlwaysSet{feature_2},
              .locally_inferred_features = FeatureMayAlwaysSet{feature_1}})});

  // Triggered counterparts now exclude rule_1 which was previously fulfilled.
  EXPECT_THAT(
      state.make_triggered_counterparts(unfulfilled, *context.kinds),
      testing::UnorderedElementsAre(
          context.kinds->get_triggered(unfulfilled, rule_2.get())));
}

} // namespace marianatrench

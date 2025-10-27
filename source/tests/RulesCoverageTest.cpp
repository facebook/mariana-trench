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

namespace {

Model make_model_with_source_argument1(
    Context& context,
    const Method* method,
    const Kind* source) {
  return Model(
      method,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* config overrides */ {},
      /* generations */
      {{AccessPath(Root(Root::Kind::Argument, 1)),
        test::make_leaf_taint_config(source)}},
      /* parameter_sources */ {},
      /* sinks */ {});
}

Model make_model_with_sink_argument0(
    Context& context,
    const Method* method,
    const Kind* sink) {
  return Model(
      method,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* config overrides */ {},
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */
      {{AccessPath(Root(Root::Kind::Argument, 0)),
        test::make_leaf_taint_config(sink)}});
}

Model make_model_with_transform_argument1to0(
    Context& context,
    const Method* method,
    const TransformList* transforms) {
  const auto* transform_kind1 = context.kind_factory->transform_kind(
      /* base_kind */ context.kind_factory->local_argument(0),
      /* local_transforms */ transforms,
      /* global_transforms */ nullptr);
  return Model(
      method,
      context,
      /* modes */ {},
      /* frozen */ {},
      /* config overrides */ {},
      /* generations */ {},
      /* parameter_sources */ {},
      /* sinks */ {},
      /* propagations */
      {test::make_propagation_config(
          transform_kind1,
          /* input_path */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* output_path */ AccessPath(Root(Root::Kind::Argument, 0)))});
}

} // namespace

class RulesCoverageTest : public test::Test {};

TEST_F(RulesCoverageTest, TestCoverageInfo) {
  auto context = test::make_empty_context();

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

  DexStore store("stores");
  Scope scope;
  store.add_classes(scope);

  // NOTE: When adding models for this method to the registry, the caller port
  // needs to be valid, i.e. Argument(0-1) only, and no Return. The make_model_*
  // helpers are named to indicate the hard-coded caller ports within them.
  const auto* method = context.methods->create(
      marianatrench::redex::create_void_method(
          scope,
          /* class_name */ "LClass;",
          /* method_name */ "returns_void",
          /* parameter_types */ "I",
          /* return_type*/ "V",
          /* super */ nullptr));

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
  {
    auto empty_registry = Registry(context);
    EXPECT_EQ(
        RulesCoverage::compute(empty_registry, rules),
        (RulesCoverage{
            /* covered_rules */ {},
            /* non_covered_rule_codes */ {1, 2, 3},
        }));
  }

  // Simple source-sink rule (no transforms).
  {
    auto registry = Registry(
        context,
        /* models */
        std::vector<Model>{
            make_model_with_source_argument1(context, method, source1),
            make_model_with_sink_argument0(context, method, sink1)},
        /* field_models */ {},
        /* literal_models */ {});
    EXPECT_EQ(
        RulesCoverage::compute(registry, rules),
        (RulesCoverage{
            /* covered_rules */
            {{1,
              CoveredRule{
                  .code = 1,
                  .used_sources = {source1},
                  .used_sinks = {sink1},
                  .used_transforms = {}}}},
            /* non_covered_rule_codes */ {2, 3}}));
  }

  // Source-sink rule with transforms.
  {
    auto registry = Registry(
        context,
        /* models */
        {make_model_with_source_argument1(context, method, source1),
         make_model_with_sink_argument0(context, method, sink1),
         make_model_with_transform_argument1to0(
             context, method, transform_list1)},
        /* field_models */ {},
        /* literal_models */ {});
    EXPECT_EQ(
        RulesCoverage::compute(registry, rules),
        (RulesCoverage{
            /* covered_rules */
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
            /* non_covered_rule_codes */ {3}}));
  }

  // Multi-source rule with partial source/sink coverage.
  // For these rules, *both* branches/labels must have sources/sinks in the
  // input.
  {
    auto source1_sinkA_registry = Registry(
        context,
        /* models */
        std::vector<Model>{
            make_model_with_source_argument1(context, method, source1),
            make_model_with_sink_argument0(context, method, partial_sink_a)},
        /* field_models */ {},
        /* literal_models */ {});
    EXPECT_EQ(
        RulesCoverage::compute(source1_sinkA_registry, rules),
        (RulesCoverage{/* covered_rules */ {},
                       /* non_covered_rule_codes */ {1, 2, 3}}));

    auto multi_source_registry = Registry(
        context,
        /* models */
        {make_model_with_source_argument1(context, method, source1),
         make_model_with_source_argument1(context, method, source2),
         make_model_with_sink_argument0(context, method, partial_sink_a),
         make_model_with_sink_argument0(context, method, partial_sink_b)},
        /* field_models */ {},
        /* literal_models */ {});
    EXPECT_EQ(
        RulesCoverage::compute(multi_source_registry, rules),
        (RulesCoverage{
            /* covered_rules */
            {{3,
              CoveredRule{
                  .code = 3,
                  .used_sources = {source1, source2},
                  .used_sinks = {partial_sink_a, partial_sink_b},
                  .used_transforms = {}}}},
            /* non_covered_rule_codes */ {1, 2}}));
  }
}

} // namespace marianatrench

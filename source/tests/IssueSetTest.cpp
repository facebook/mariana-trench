/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/IssueSet.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/SourceSinkRule.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class IssueSetTest : public test::Test {};

TEST_F(IssueSetTest, Insertion) {
  auto context = test::make_empty_context();

  const auto* source_kind = context.kind_factory->get("TestSource");
  const auto* other_source_kind = context.kind_factory->get("OtherSource");
  const auto* sink_kind = context.kind_factory->get("TestSink");
  const auto* other_sink_kind = context.kind_factory->get("OtherSink");

  SourceSinkRule rule_1(
      "rule 1",
      1,
      "description",
      {source_kind},
      {sink_kind},
      /* transforms */ nullptr);
  SourceSinkRule rule_2(
      "rule 2",
      2,
      "description",
      {other_source_kind},
      {other_sink_kind},
      /* transforms */ nullptr);
  SourceSinkRule rule_3(
      "rule 3",
      3,
      "description",
      {source_kind},
      {other_sink_kind},
      /* transforms */ nullptr);

  const auto* position_1 = context.positions->get(std::nullopt, 1);
  const auto* position_2 = context.positions->get(std::nullopt, 2);

  Scope scope;
  auto* one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));
  auto* two = context.methods->create(
      redex::create_void_method(scope, "LOther;", "two"));
  auto* one_origin = context.origin_factory->method_origin(one);
  auto* two_origin = context.origin_factory->method_origin(two);

  IssueSet set = {};
  EXPECT_EQ(set, IssueSet{});

  set.add(Issue(
      /* source */ Taint{test::make_leaf_taint_config(source_kind)},
      /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
      &rule_1,
      /* callee */ std::string(k_return_callee),
      /* sink_index */ 0,
      position_1));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{test::make_leaf_taint_config(source_kind)},
              /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
              &rule_1,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_1),
      }));

  set.add(Issue(
      /* source */ Taint{test::make_leaf_taint_config(other_source_kind)},
      /* sink */ Taint{test::make_leaf_taint_config(other_sink_kind)},
      &rule_2,
      /* callee */ std::string(k_return_callee),
      /* sink_index */ 0,
      position_1));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{test::make_leaf_taint_config(source_kind)},
              /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
              &rule_1,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_1),
          Issue(
              /* source */ Taint{test::make_leaf_taint_config(
                  other_source_kind)},
              /* sink */ Taint{test::make_leaf_taint_config(other_sink_kind)},
              &rule_2,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_1),
      }));

  set.add(Issue(
      /* source */ Taint{test::make_taint_config(
          /* kind */ other_source_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = one,
              .call_position = context.positions->unknown(),
              .distance = 1,
              .origins = OriginSet{one_origin},
              .call_kind = CallKind::callsite(),
          })},
      /* sink */ Taint{test::make_leaf_taint_config(other_sink_kind)},
      &rule_2,
      /* callee */ std::string(k_return_callee),
      /* sink_index */ 0,
      position_1));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{test::make_leaf_taint_config(source_kind)},
              /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
              &rule_1,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_1),
          Issue(
              /* source */
              Taint{
                  test::make_leaf_taint_config(other_source_kind),
                  test::make_taint_config(
                      /* kind */ other_source_kind,
                      test::FrameProperties{
                          .callee_port = AccessPath(Root(Root::Kind::Return)),
                          .callee = one,
                          .call_position = context.positions->unknown(),
                          .distance = 1,
                          .origins = OriginSet{one_origin},
                          .call_kind = CallKind::callsite(),
                      }),
              },
              /* sink */ Taint{test::make_leaf_taint_config(other_sink_kind)},
              &rule_2,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_1),
      }));

  set.add(Issue(
      /* source */ Taint{test::make_leaf_taint_config(other_source_kind)},
      /* sink */
      Taint{test::make_taint_config(
          /* kind */ other_sink_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = two,
              .call_position = context.positions->unknown(),
              .distance = 2,
              .origins = OriginSet{two_origin},
              .call_kind = CallKind::callsite(),
          })},
      &rule_2,
      /* callee */ std::string(k_return_callee),
      /* sink_index */ 0,
      position_1));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{test::make_leaf_taint_config(source_kind)},
              /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
              &rule_1,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_1),
          Issue(
              /* source */
              Taint{
                  test::make_leaf_taint_config(other_source_kind),
                  test::make_taint_config(
                      /* kind */ other_source_kind,
                      test::FrameProperties{
                          .callee_port = AccessPath(Root(Root::Kind::Return)),
                          .callee = one,
                          .call_position = context.positions->unknown(),
                          .distance = 1,
                          .origins = OriginSet{one_origin},
                          .call_kind = CallKind::callsite(),
                      }),
              },
              /* sink */
              Taint{
                  test::make_leaf_taint_config(other_sink_kind),
              },
              &rule_2,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_1),
          Issue(
              /* source */ Taint{test::make_leaf_taint_config(
                  other_source_kind)},
              /* sink */
              Taint{test::make_taint_config(
                  /* kind */ other_sink_kind,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Return)),
                      .callee = two,
                      .call_position = context.positions->unknown(),
                      .distance = 2,
                      .origins = OriginSet{two_origin},
                      .call_kind = CallKind::callsite(),
                  })},
              &rule_2,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_1)}));

  set.add(Issue(
      /* source */ Taint{test::make_taint_config(
          /* kind */ other_source_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = two,
              .call_position = context.positions->unknown(),
              .distance = 3,
              .origins = OriginSet{two_origin},
              .call_kind = CallKind::callsite(),
          })},
      /* sink */ Taint{test::make_leaf_taint_config(other_sink_kind)},
      &rule_2,
      /* callee */ std::string(k_return_callee),
      /* sink_index */ 0,
      position_1));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{test::make_leaf_taint_config(source_kind)},
              /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
              &rule_1,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_1),
          Issue(
              /* source */
              Taint{
                  test::make_leaf_taint_config(other_source_kind),
                  test::make_taint_config(
                      /* kind */ other_source_kind,
                      test::FrameProperties{
                          .callee_port = AccessPath(Root(Root::Kind::Return)),
                          .callee = one,
                          .call_position = context.positions->unknown(),
                          .distance = 1,
                          .origins = OriginSet{one_origin},
                          .call_kind = CallKind::callsite(),
                      }),
                  test::make_taint_config(
                      /* kind */ other_source_kind,
                      test::FrameProperties{
                          .callee_port = AccessPath(Root(Root::Kind::Return)),
                          .callee = two,
                          .call_position = context.positions->unknown(),
                          .distance = 3,
                          .origins = OriginSet{two_origin},
                          .call_kind = CallKind::callsite(),
                      }),
              },
              /* sink */
              Taint{
                  test::make_leaf_taint_config(other_sink_kind),
              },
              &rule_2,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_1),
          Issue(
              /* source */ Taint{test::make_leaf_taint_config(
                  other_source_kind)},
              /* sink */
              Taint{test::make_taint_config(
                  /* kind */ other_sink_kind,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Return)),
                      .callee = two,
                      .call_position = context.positions->unknown(),
                      .distance = 2,
                      .origins = OriginSet{two_origin},
                      .call_kind = CallKind::callsite(),
                  })},
              &rule_2,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_1)}));

  set.add(Issue(
      /* source */ Taint{test::make_leaf_taint_config(source_kind)},
      /* sink */ Taint{test::make_leaf_taint_config(other_sink_kind)},
      &rule_1,
      /* callee */ std::string(k_return_callee),
      /* sink_index */ 1,
      position_1));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{test::make_leaf_taint_config(source_kind)},
              /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
              &rule_1,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_1),
          Issue(
              /* source */ Taint{test::make_leaf_taint_config(source_kind)},
              /* sink */ Taint{test::make_leaf_taint_config(other_sink_kind)},
              &rule_1,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 1,
              position_1),
          Issue(
              /* source */
              Taint{
                  test::make_leaf_taint_config(other_source_kind),
                  test::make_taint_config(
                      /* kind */ other_source_kind,
                      test::FrameProperties{
                          .callee_port = AccessPath(Root(Root::Kind::Return)),
                          .callee = one,
                          .call_position = context.positions->unknown(),
                          .distance = 1,
                          .origins = OriginSet{one_origin},
                          .call_kind = CallKind::callsite(),
                      }),
                  test::make_taint_config(
                      /* kind */ other_source_kind,
                      test::FrameProperties{
                          .callee_port = AccessPath(Root(Root::Kind::Return)),
                          .callee = two,
                          .call_position = context.positions->unknown(),
                          .distance = 3,
                          .origins = OriginSet{two_origin},
                          .call_kind = CallKind::callsite(),
                      }),
              },
              /* sink */
              Taint{
                  test::make_leaf_taint_config(other_sink_kind),
              },
              &rule_2,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_1),
          Issue(
              /* source */ Taint{test::make_leaf_taint_config(
                  other_source_kind)},
              /* sink */
              Taint{test::make_taint_config(
                  /* kind */ other_sink_kind,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Return)),
                      .callee = two,
                      .call_position = context.positions->unknown(),
                      .distance = 2,
                      .origins = OriginSet{two_origin},
                      .call_kind = CallKind::callsite(),
                  })},
              &rule_2,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_1)}));

  set = {};
  set.add(Issue(
      /* source */ Taint{test::make_leaf_taint_config(source_kind)},
      /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
      &rule_1,
      /* callee */ std::string(k_return_callee),
      /* sink_index */ 0,
      position_1));
  set.add(Issue(
      /* source */ Taint{test::make_leaf_taint_config(source_kind)},
      /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
      &rule_1,
      /* callee */ std::string(k_return_callee),
      /* sink_index */ 1,
      position_1));
  // Issues shouldn't be joined since sink_indices are different
  EXPECT_EQ(set.size(), 2);

  set = {};
  set.add(Issue(
      /* source */ Taint{test::make_leaf_taint_config(source_kind)},
      /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
      &rule_1,
      /* callee */ std::string(k_return_callee),
      /* sink_index */ 0,
      position_1));
  set.add(Issue(
      /* source */ Taint{test::make_leaf_taint_config(source_kind)},
      /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
      &rule_1,
      /* callee */ std::string(k_return_callee),
      /* sink_index */ 0,
      position_2));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{test::make_leaf_taint_config(source_kind)},
              /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
              &rule_1,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_1),
          Issue(
              /* source */ Taint{test::make_leaf_taint_config(source_kind)},
              /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
              &rule_1,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_2),
      }));
  // Merging with existing issues that have inferred_features == bottom()
  // should retain the "always"-ness property of the issue.
  set.add(Issue(
      /* source */ Taint{test::make_leaf_taint_config(
          source_kind,
          /* inferred_features */
          FeatureMayAlwaysSet::make_always(
              {context.feature_factory->get("Feature")}),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ FeatureSet::bottom(),
          /* origins */ {})},
      /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
      &rule_1,
      /* callee */ std::string(k_return_callee),
      /* sink_index */ 0,
      position_2));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{test::make_leaf_taint_config(source_kind)},
              /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
              &rule_1,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_1),
          Issue(
              /* source */ Taint{test::make_leaf_taint_config(
                  source_kind,
                  /* inferred_features */
                  FeatureMayAlwaysSet::make_always(
                      {context.feature_factory->get("Feature")}),
                  /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* user_features */ FeatureSet::bottom(),
                  /* origins */ {})},
              /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
              &rule_1,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_2),
      }));
  // Merging with issues that have inferred_features != bottom() would convert
  // "always" to "may" only for features that are not shared across the issues.
  set.add(Issue(
      /* source */ Taint{test::make_leaf_taint_config(
          source_kind,
          /* inferred_features */
          FeatureMayAlwaysSet::make_always(FeatureSet{
              context.feature_factory->get("Feature"),
              context.feature_factory->get("Feature2")}),
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          /* user_features */ FeatureSet::bottom(),
          /* origins */ {})},
      /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
      &rule_1,
      /* callee */ std::string(k_return_callee),
      /* sink_index */ 0,
      position_2));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{test::make_leaf_taint_config(source_kind)},
              /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
              &rule_1,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_1),
          Issue(
              /* source */ Taint{test::make_leaf_taint_config(
                  source_kind,
                  /* inferred_features */
                  FeatureMayAlwaysSet(
                      /* may */ FeatureSet{context.feature_factory->get(
                          "Feature2")},
                      /* always */
                      FeatureSet{context.feature_factory->get("Feature")}),
                  /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
                  /* user_features */ FeatureSet::bottom(),
                  /* origins */ {})},
              /* sink */ Taint{test::make_leaf_taint_config(sink_kind)},
              &rule_1,
              /* callee */ std::string(k_return_callee),
              /* sink_index */ 0,
              position_2),
      }));
}

} // namespace marianatrench

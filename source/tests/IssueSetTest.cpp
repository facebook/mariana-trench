/*
 * Copyright (c) Facebook, Inc. and its affiliates.
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

  const auto* source_kind = context.kinds->get("TestSource");
  const auto* other_source_kind = context.kinds->get("OtherSource");
  const auto* sink_kind = context.kinds->get("TestSink");
  const auto* other_sink_kind = context.kinds->get("OtherSink");

  SourceSinkRule rule_1("rule 1", 1, "description", {source_kind}, {sink_kind});
  SourceSinkRule rule_2(
      "rule 2", 2, "description", {other_source_kind}, {other_sink_kind});
  SourceSinkRule rule_3(
      "rule 3", 3, "description", {source_kind}, {other_sink_kind});

  const auto* position_1 = context.positions->get(std::nullopt, 1);
  const auto* position_2 = context.positions->get(std::nullopt, 2);

  Scope scope;
  auto* one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));
  auto* two = context.methods->create(
      redex::create_void_method(scope, "LOther;", "two"));

  IssueSet set = {};
  EXPECT_EQ(set, IssueSet{});

  set.add(Issue(
      /* source */ Taint{Frame::leaf(source_kind)},
      /* sink */ Taint{Frame::leaf(sink_kind)},
      &rule_1,
      position_1));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1,
              position_1),
      }));

  set.add(Issue(
      /* source */ Taint{Frame::leaf(other_source_kind)},
      /* sink */ Taint{Frame::leaf(other_sink_kind)},
      &rule_2,
      position_1));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1,
              position_1),
          Issue(
              /* source */ Taint{Frame::leaf(other_source_kind)},
              /* sink */ Taint{Frame::leaf(other_sink_kind)},
              &rule_2,
              position_1),
      }));

  set.add(Issue(
      /* source */ Taint{Frame(
          /* kind */ other_source_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {})},
      /* sink */ Taint{Frame::leaf(other_sink_kind)},
      &rule_2,
      position_1));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1,
              position_1),
          Issue(
              /* source */
              Taint{
                  Frame::leaf(other_source_kind),
                  Frame(
                      /* kind */ other_source_kind,
                      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                      /* callee */ one,
                      /* call_position */ context.positions->unknown(),
                      /* distance */ 1,
                      /* origins */ MethodSet{one},
                      /* inferred_features */ {},
                      /* user_features */ {},
                      /* via_type_of_ports */ {},
                      /* local_positions */ {}),
              },
              /* sink */ Taint{Frame::leaf(other_sink_kind)},
              &rule_2,
              position_1),
      }));

  set.add(Issue(
      /* source */ Taint{Frame::leaf(other_source_kind)},
      /* sink */
      Taint{Frame(
          /* kind */ other_sink_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ two,
          /* call_position */ context.positions->unknown(),
          /* distance */ 2,
          /* origins */ MethodSet{two},
          /* inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {})},
      &rule_2,
      position_1));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1,
              position_1),
          Issue(
              /* source */
              Taint{
                  Frame::leaf(other_source_kind),
                  Frame(
                      /* kind */ other_source_kind,
                      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                      /* callee */ one,
                      /* call_position */ context.positions->unknown(),
                      /* distance */ 1,
                      /* origins */ MethodSet{one},
                      /* inferred_features */ {},
                      /* user_features */ {},
                      /* via_type_of_ports */ {},
                      /* local_positions */ {}),
              },
              /* sink */
              Taint{
                  Frame::leaf(other_sink_kind),
                  Frame(
                      /* kind */ other_sink_kind,
                      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                      /* callee */ two,
                      /* call_position */ context.positions->unknown(),
                      /* distance */ 2,
                      /* origins */ MethodSet{two},
                      /* inferred_features */ {},
                      /* user_features */ {},
                      /* via_type_of_ports */ {},
                      /* local_positions */ {}),
              },
              &rule_2,
              position_1),
      }));

  set.add(Issue(
      /* source */ Taint{Frame(
          /* kind */ other_source_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ two,
          /* call_position */ context.positions->unknown(),
          /* distance */ 3,
          /* origins */ MethodSet{two},
          /* inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {})},
      /* sink */ Taint{Frame::leaf(other_sink_kind)},
      &rule_2,
      position_1));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1,
              position_1),
          Issue(
              /* source */
              Taint{
                  Frame::leaf(other_source_kind),
                  Frame(
                      /* kind */ other_source_kind,
                      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                      /* callee */ one,
                      /* call_position */ context.positions->unknown(),
                      /* distance */ 1,
                      /* origins */ MethodSet{one},
                      /* inferred_features */ {},
                      /* user_features */ {},
                      /* via_type_of_ports */ {},
                      /* local_positions */ {}),
                  Frame(
                      /* kind */ other_source_kind,
                      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                      /* callee */ two,
                      /* call_position */ context.positions->unknown(),
                      /* distance */ 3,
                      /* origins */ MethodSet{two},
                      /* inferred_features */ {},
                      /* user_features */ {},
                      /* via_type_of_ports */ {},
                      /* local_positions */ {}),
              },
              /* sink */
              Taint{
                  Frame::leaf(other_sink_kind),
                  Frame(
                      /* kind */ other_sink_kind,
                      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                      /* callee */ two,
                      /* call_position */ context.positions->unknown(),
                      /* distance */ 2,
                      /* origins */ MethodSet{two},
                      /* inferred_features */ {},
                      /* user_features */ {},
                      /* via_type_of_ports */ {},
                      /* local_positions */ {}),
              },
              &rule_2,
              position_1),
      }));

  set.add(Issue(
      /* source */ Taint{Frame::leaf(source_kind)},
      /* sink */ Taint{Frame::leaf(other_sink_kind)},
      &rule_3,
      position_1));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1,
              position_1),
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(other_sink_kind)},
              &rule_3,
              position_1),
          Issue(
              /* source */
              Taint{
                  Frame::leaf(other_source_kind),
                  Frame(
                      /* kind */ other_source_kind,
                      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                      /* callee */ one,
                      /* call_position */ context.positions->unknown(),
                      /* distance */ 1,
                      /* origins */ MethodSet{one},
                      /* inferred_features */ {},
                      /* user_features */ {},
                      /* via_type_of_ports */ {},
                      /* local_positions */ {}),
                  Frame(
                      /* kind */ other_source_kind,
                      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                      /* callee */ two,
                      /* call_position */ context.positions->unknown(),
                      /* distance */ 3,
                      /* origins */ MethodSet{two},
                      /* inferred_features */ {},
                      /* user_features */ {},
                      /* via_type_of_ports */ {},
                      /* local_positions */ {}),
              },
              /* sink */
              Taint{
                  Frame::leaf(other_sink_kind),
                  Frame(
                      /* kind */ other_sink_kind,
                      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                      /* callee */ two,
                      /* call_position */ context.positions->unknown(),
                      /* distance */ 2,
                      /* origins */ MethodSet{two},
                      /* inferred_features */ {},
                      /* user_features */ {},
                      /* via_type_of_ports */ {},
                      /* local_positions */ {}),
              },
              &rule_2,
              position_1),
      }));

  set = {};
  set.add(Issue(
      /* source */ Taint{Frame::leaf(source_kind)},
      /* sink */ Taint{Frame::leaf(sink_kind)},
      &rule_1,
      position_1));
  set.add(Issue(
      /* source */ Taint{Frame::leaf(source_kind)},
      /* sink */ Taint{Frame::leaf(sink_kind)},
      &rule_1,
      position_2));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1,
              position_1),
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1,
              position_2),
      }));

  // Merging with existing issues that have inferred_features == bottom()
  // should retain the "always"-ness property of the issue.
  set.add(Issue(
      /* source */ Taint{Frame::leaf(
          source_kind,
          FeatureMayAlwaysSet::make_always(
              {context.features->get("Feature")}))},
      /* sink */ Taint{Frame::leaf(sink_kind)},
      &rule_1,
      position_2));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1,
              position_1),
          Issue(
              /* source */ Taint{Frame::leaf(
                  source_kind,
                  FeatureMayAlwaysSet::make_always(
                      {context.features->get("Feature")}))},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1,
              position_2),
      }));

  // Merging with issues that have inferred_features != bottom() would convert
  // "always" to "may" only for features that are not shared across the issues.
  set.add(Issue(
      /* source */ Taint{Frame::leaf(
          source_kind,
          FeatureMayAlwaysSet::make_always(FeatureSet{
              context.features->get("Feature"),
              context.features->get("Feature2")}))},
      /* sink */ Taint{Frame::leaf(sink_kind)},
      &rule_1,
      position_2));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1,
              position_1),
          Issue(
              /* source */ Taint{Frame::leaf(
                  source_kind,
                  FeatureMayAlwaysSet(
                      /* may */ FeatureSet{context.features->get("Feature2")},
                      /* always */
                      FeatureSet{context.features->get("Feature")}))},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1,
              position_2),
      }));
}

} // namespace marianatrench

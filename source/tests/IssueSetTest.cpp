/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/IssueSet.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class IssueSetTest : public test::Test {};

TEST_F(IssueSetTest, Insertion) {
  auto context = test::make_empty_context();

  const auto* source_kind = context.kinds->get("TestSource");
  const auto* other_source_kind = context.kinds->get("OtherSource");
  const auto* sink_kind = context.kinds->get("TestSink");
  const auto* other_sink_kind = context.kinds->get("OtherSink");

  Rule rule_1("rule 1", 1, "description", {source_kind}, {sink_kind});
  Rule rule_2(
      "rule 2", 2, "description", {other_source_kind}, {other_sink_kind});
  Rule rule_3("rule 3", 3, "description", {source_kind}, {other_sink_kind});

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
      &rule_1));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1),
      }));

  set.add(Issue(
      /* source */ Taint{Frame::leaf(other_source_kind)},
      /* sink */ Taint{Frame::leaf(other_sink_kind)},
      &rule_2));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1),
          Issue(
              /* source */ Taint{Frame::leaf(other_source_kind)},
              /* sink */ Taint{Frame::leaf(other_sink_kind)},
              &rule_2),
      }));

  set.add(Issue(
      /* source */ Taint{Frame(
          /* kind */ other_source_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* features */ {},
          /* local_positions */ {})},
      /* sink */ Taint{Frame::leaf(other_sink_kind)},
      &rule_2));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1),
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
                      /* features */ {},
                      /* local_positions */ {}),
              },
              /* sink */ Taint{Frame::leaf(other_sink_kind)},
              &rule_2),
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
          /* features */ {},
          /* local_positions */ {})},
      &rule_2));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1),
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
                      /* features */ {},
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
                      /* features */ {},
                      /* local_positions */ {}),
              },
              &rule_2),
      }));

  set.add(Issue(
      /* source */ Taint{Frame(
          /* kind */ other_source_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ two,
          /* call_position */ context.positions->unknown(),
          /* distance */ 3,
          /* origins */ MethodSet{two},
          /* features */ {},
          /* local_positions */ {})},
      /* sink */ Taint{Frame::leaf(other_sink_kind)},
      &rule_2));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1),
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
                      /* features */ {},
                      /* local_positions */ {}),
                  Frame(
                      /* kind */ other_source_kind,
                      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                      /* callee */ two,
                      /* call_position */ context.positions->unknown(),
                      /* distance */ 3,
                      /* origins */ MethodSet{two},
                      /* features */ {},
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
                      /* features */ {},
                      /* local_positions */ {}),
              },
              &rule_2),
      }));

  set.add(Issue(
      /* source */ Taint{Frame::leaf(source_kind)},
      /* sink */ Taint{Frame::leaf(other_sink_kind)},
      &rule_3));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(sink_kind)},
              &rule_1),
          Issue(
              /* source */ Taint{Frame::leaf(source_kind)},
              /* sink */ Taint{Frame::leaf(other_sink_kind)},
              &rule_3),
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
                      /* features */ {},
                      /* local_positions */ {}),
                  Frame(
                      /* kind */ other_source_kind,
                      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                      /* callee */ two,
                      /* call_position */ context.positions->unknown(),
                      /* distance */ 3,
                      /* origins */ MethodSet{two},
                      /* features */ {},
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
                      /* features */ {},
                      /* local_positions */ {}),
              },
              &rule_2),
      }));
}

} // namespace marianatrench

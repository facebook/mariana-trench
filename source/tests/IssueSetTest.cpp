// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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

  Rule rule(
      "rule",
      1,
      "description",
      {source_kind, other_source_kind},
      {sink_kind, other_sink_kind});

  Scope scope;
  auto* one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));
  auto* two = context.methods->create(
      redex::create_void_method(scope, "LOther;", "two"));

  IssueSet set = {};
  EXPECT_EQ(set, IssueSet{});

  set.add(Issue(
      /* source */ FrameSet{Frame::leaf(source_kind)},
      /* sink */ FrameSet{Frame::leaf(sink_kind)},
      &rule));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ FrameSet{Frame::leaf(source_kind)},
              /* sink */ FrameSet{Frame::leaf(sink_kind)},
              &rule),
      }));

  set.add(Issue(
      /* source */ FrameSet{Frame::leaf(other_source_kind)},
      /* sink */ FrameSet{Frame::leaf(other_sink_kind)},
      &rule));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ FrameSet{Frame::leaf(source_kind)},
              /* sink */ FrameSet{Frame::leaf(sink_kind)},
              &rule),
          Issue(
              /* source */ FrameSet{Frame::leaf(other_source_kind)},
              /* sink */ FrameSet{Frame::leaf(other_sink_kind)},
              &rule),
      }));

  set.add(Issue(
      /* source */ FrameSet{Frame(
          /* kind */ other_source_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* features */ {},
          /* local_positions */ {})},
      /* sink */ FrameSet{Frame::leaf(other_sink_kind)},
      &rule));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ FrameSet{Frame::leaf(source_kind)},
              /* sink */ FrameSet{Frame::leaf(sink_kind)},
              &rule),
          Issue(
              /* source */
              FrameSet{
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
              /* sink */ FrameSet{Frame::leaf(other_sink_kind)},
              &rule),
      }));

  set.add(Issue(
      /* source */ FrameSet{Frame::leaf(other_source_kind)},
      /* sink */
      FrameSet{Frame(
          /* kind */ other_sink_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ two,
          /* call_position */ context.positions->unknown(),
          /* distance */ 2,
          /* origins */ MethodSet{two},
          /* features */ {},
          /* local_positions */ {})},
      &rule));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ FrameSet{Frame::leaf(source_kind)},
              /* sink */ FrameSet{Frame::leaf(sink_kind)},
              &rule),
          Issue(
              /* source */
              FrameSet{
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
              FrameSet{
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
              &rule),
      }));

  set.add(Issue(
      /* source */ FrameSet{Frame(
          /* kind */ other_source_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ two,
          /* call_position */ context.positions->unknown(),
          /* distance */ 3,
          /* origins */ MethodSet{two},
          /* features */ {},
          /* local_positions */ {})},
      /* sink */ FrameSet{Frame::leaf(other_sink_kind)},
      &rule));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ FrameSet{Frame::leaf(source_kind)},
              /* sink */ FrameSet{Frame::leaf(sink_kind)},
              &rule),
          Issue(
              /* source */
              FrameSet{
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
              FrameSet{
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
              &rule),
      }));

  set.add(Issue(
      /* source */ FrameSet{Frame::leaf(source_kind)},
      /* sink */ FrameSet{Frame::leaf(other_sink_kind)},
      &rule));
  EXPECT_EQ(
      set,
      (IssueSet{
          Issue(
              /* source */ FrameSet{Frame::leaf(source_kind)},
              /* sink */ FrameSet{Frame::leaf(sink_kind)},
              &rule),
          Issue(
              /* source */ FrameSet{Frame::leaf(source_kind)},
              /* sink */ FrameSet{Frame::leaf(other_sink_kind)},
              &rule),
          Issue(
              /* source */
              FrameSet{
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
              FrameSet{
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
              &rule),
      }));
}

} // namespace marianatrench

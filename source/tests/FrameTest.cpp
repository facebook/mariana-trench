/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/Frame.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class FrameTest : public test::Test {};

TEST_F(FrameTest, FrameConstructor) {
  EXPECT_TRUE(Frame().is_bottom());
  EXPECT_TRUE(Frame::bottom().is_bottom());
}

TEST_F(FrameTest, FrameLeq) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));
  auto* two = context.methods->create(
      redex::create_void_method(scope, "LOther;", "two"));

  EXPECT_TRUE(Frame::bottom().leq(Frame::bottom()));
  EXPECT_TRUE(Frame::bottom().leq(Frame(
      /* kind */ context.kinds->get("TestSource"),
      /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
      /* callee */ nullptr,
      /* call_position */ nullptr,
      /* distance */ 0,
      /* origins */ {},
      /* features */ {},
      /* local_positions */ {})));
  EXPECT_FALSE(Frame(
                   /* kind */ context.kinds->get("TestSource"),
                   /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                   /* callee */ nullptr,
                   /* call_position */ nullptr,
                   /* distance */ 0,
                   /* origins */ {},
                   /* features */ {},
                   /* local_positions */ {})
                   .leq(Frame::bottom()));

  // Compare kind.
  EXPECT_TRUE(Frame(
                  /* kind */ context.kinds->get("TestSource"),
                  /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                  /* callee */ nullptr,
                  /* call_position */ nullptr,
                  /* distance */ 0,
                  /* origins */ {},
                  /* features */ {},
                  /* local_positions */ {})
                  .leq(Frame(
                      /* kind */ context.kinds->get("TestSource"),
                      /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                      /* callee */ nullptr,
                      /* call_position */ nullptr,
                      /* distance */ 0,
                      /* origins */ {},
                      /* features */ {},
                      /* local_positions */ {})));
  EXPECT_FALSE(Frame(
                   /* kind */ context.kinds->get("TestSource"),
                   /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                   /* callee */ nullptr,
                   /* call_position */ nullptr,
                   /* distance */ 0,
                   /* origins */ {},
                   /* features */ {},
                   /* local_positions */ {})
                   .leq(Frame(
                       /* kind */ context.kinds->get("TestSink"),
                       /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                       /* callee */ nullptr,
                       /* call_position */ nullptr,
                       /* distance */ 0,
                       /* origins */ {},
                       /* features */ {},
                       /* local_positions */ {})));

  // Compare distances.
  EXPECT_TRUE(Frame(
                  /* kind */ context.kinds->get("TestSource"),
                  /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                  /* callee */ nullptr,
                  /* call_position */ nullptr,
                  /* distance */ 1,
                  /* origins */ {},
                  /* features */ {},
                  /* local_positions */ {})
                  .leq(Frame(
                      /* kind */ context.kinds->get("TestSource"),
                      /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                      /* callee */ nullptr,
                      /* call_position */ nullptr,
                      /* distance */ 0,
                      /* origins */ {},
                      /* features */ {},
                      /* local_positions */ {})));
  EXPECT_FALSE(Frame(
                   /* kind */ context.kinds->get("TestSource"),
                   /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                   /* callee */ nullptr,
                   /* call_position */ nullptr,
                   /* distance */ 0,
                   /* origins */ {},
                   /* features */ {},
                   /* local_positions */ {})
                   .leq(Frame(
                       /* kind */ context.kinds->get("TestSource"),
                       /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                       /* callee */ nullptr,
                       /* call_position */ nullptr,
                       /* distance */ 1,
                       /* origins */ {},
                       /* features */ {},
                       /* local_positions */ {})));

  // Compare origins.
  EXPECT_TRUE(Frame(
                  /* kind */ context.kinds->get("TestSource"),
                  /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                  /* callee */ nullptr,
                  /* call_position */ nullptr,
                  /* distance */ 0,
                  /* origins */ MethodSet{one},
                  /* features */ {},
                  /* local_positions */ {})
                  .leq(Frame(
                      /* kind */ context.kinds->get("TestSource"),
                      /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                      /* callee */ nullptr,
                      /* call_position */ nullptr,
                      /* distance */ 0,
                      /* origins */ MethodSet{one, two},
                      /* features */ {},
                      /* local_positions */ {})));
  EXPECT_FALSE(Frame(
                   /* kind */ context.kinds->get("TestSource"),
                   /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                   /* callee */ nullptr,
                   /* call_position */ nullptr,
                   /* distance */ 0,
                   /* origins */ MethodSet{one, two},
                   /* features */ {},
                   /* local_positions */ {})
                   .leq(Frame(
                       /* kind */ context.kinds->get("TestSource"),
                       /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                       /* callee */ nullptr,
                       /* call_position */ nullptr,
                       /* distance */ 0,
                       /* origins */ MethodSet{one},
                       /* features */ {},
                       /* local_positions */ {})));

  // Compare features.
  EXPECT_TRUE(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* features */
          FeatureMayAlwaysSet::make_may({context.features->get("FeatureOne")}),
          /* local_positions */ {})
          .leq(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* features */
              FeatureMayAlwaysSet::make_may(
                  {context.features->get("FeatureOne"),
                   context.features->get("FeatureTwo")}),
              /* local_positions */ {})));
  EXPECT_FALSE(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* features */
          FeatureMayAlwaysSet::make_may({context.features->get("FeatureOne"),
                                         context.features->get("FeatureTwo")}),
          /* local_positions */ {})
          .leq(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* features */
              FeatureMayAlwaysSet::make_may(
                  {context.features->get("FeatureOne")}),
              /* local_positions */ {})));

  // callee_port, callee and call_position must be equal for non-artificial
  // taint.
  EXPECT_TRUE(Frame(
                  /* kind */ context.kinds->get("TestSource"),
                  /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                  /* callee */ one,
                  /* call_position */ context.positions->unknown(),
                  /* distance */ 1,
                  /* origins */ MethodSet{one},
                  /* features */ {},
                  /* local_positions */ {})
                  .leq(Frame(
                      /* kind */ context.kinds->get("TestSource"),
                      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                      /* callee */ one,
                      /* call_position */ context.positions->unknown(),
                      /* distance */ 1,
                      /* origins */ MethodSet{one},
                      /* features */ {},
                      /* local_positions */ {})));
  EXPECT_FALSE(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* features */ {},
          /* local_positions */ {})
          .leq(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 1,
              /* origins */ MethodSet{one},
              /* features */ {},
              /* local_positions */ {})));
  EXPECT_FALSE(Frame(
                   /* kind */ context.kinds->get("TestSource"),
                   /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                   /* callee */ one,
                   /* call_position */ context.positions->unknown(),
                   /* distance */ 1,
                   /* origins */ MethodSet{one},
                   /* features */ {},
                   /* local_positions */ {})
                   .leq(Frame(
                       /* kind */ context.kinds->get("TestSource"),
                       /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                       /* callee */ two,
                       /* call_position */ context.positions->unknown(),
                       /* distance */ 1,
                       /* origins */ MethodSet{one},
                       /* features */ {},
                       /* local_positions */ {})));
  EXPECT_FALSE(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* features */ {},
          /* local_positions */ {})
          .leq(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->get("Test.java", 1),
              /* distance */ 1,
              /* origins */ MethodSet{one},
              /* features */ {},
              /* local_positions */ {})));

  // For artificial sources, compare the common prefix of callee ports.
  EXPECT_TRUE(
      Frame(
          /* kind */ Kinds::artificial_source(),
          /* callee_port */
          AccessPath(
              Root(Root::Kind::Argument, 0), Path{DexString::make_string("x")}),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* features */ {},
          /* local_positions */ {})
          .leq(Frame(
              /* kind */ Kinds::artificial_source(),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* features */ {},
              /* local_positions */ {})));
  EXPECT_FALSE(Frame(
                   /* kind */ Kinds::artificial_source(),
                   /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
                   /* callee */ nullptr,
                   /* call_position */ nullptr,
                   /* distance */ 0,
                   /* origins */ {},
                   /* features */ {},
                   /* local_positions */ {})
                   .leq(Frame(
                       /* kind */ Kinds::artificial_source(),
                       /* callee_port */
                       AccessPath(
                           Root(Root::Kind::Argument, 0),
                           Path{DexString::make_string("x")}),
                       /* callee */ nullptr,
                       /* call_position */ nullptr,
                       /* distance */ 0,
                       /* origins */ {},
                       /* features */ {},
                       /* local_positions */ {})));
}

TEST_F(FrameTest, FrameEquals) {
  auto context = test::make_empty_context();

  EXPECT_TRUE(Frame::bottom().equals(Frame::bottom()));
  EXPECT_FALSE(Frame::bottom().equals(Frame(
      /* kind */ context.kinds->get("TestSource"),
      /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
      /* callee */ nullptr,
      /* call_position */ nullptr,
      /* distance */ 0,
      /* origins */ {},
      /* features */ {},
      /* local_positions */ {})));
  EXPECT_FALSE(Frame(
                   /* kind */ context.kinds->get("TestSource"),
                   /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                   /* callee */ nullptr,
                   /* call_position */ nullptr,
                   /* distance */ 0,
                   /* origins */ {},
                   /* features */ {},
                   /* local_positions */ {})
                   .equals(Frame::bottom()));
}

TEST_F(FrameTest, FrameJoin) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));
  auto* two = context.methods->create(
      redex::create_void_method(scope, "LOther;", "two"));

  EXPECT_EQ(Frame::bottom().join(Frame::bottom()), Frame::bottom());
  EXPECT_EQ(
      Frame::bottom().join(Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* features */ {},
          /* local_positions */ {})),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* features */ {},
          /* local_positions */ {}));
  EXPECT_EQ(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* features */ {},
          /* local_positions */ {})
          .join(Frame::bottom()),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* features */ {},
          /* local_positions */ {}));

  // Test incompatible joins.
  EXPECT_THROW(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* features */ {},
          /* local_positions */ {})
          .join_with(Frame(
              /* kind */ context.kinds->get("TestSink"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* features */ {},
              /* local_positions */ {})),
      std::exception);
  EXPECT_THROW(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* features */ {},
          /* local_positions */ {})
          .join_with(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Anchor)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* features */ {},
              /* local_positions */ {})),
      std::exception);
  EXPECT_THROW(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ {},
          /* features */ {},
          /* local_positions */ {})
          .join_with(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ two,
              /* call_position */ context.positions->unknown(),
              /* distance */ 1,
              /* origins */ {},
              /* features */ {},
              /* local_positions */ {})),
      std::exception);
  EXPECT_THROW(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ {},
          /* features */ {},
          /* local_positions */ {})
          .join_with(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->get("Test.java", 1),
              /* distance */ 1,
              /* origins */ {},
              /* features */ {},
              /* local_positions */ {})),
      std::exception);

  // Minimum distance.
  EXPECT_EQ(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 2,
          /* origins */ {},
          /* features */ {},
          /* local_positions */ {})
          .join(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 1,
              /* origins */ {},
              /* features */ {},
              /* local_positions */ {})),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ {},
          /* features */ {},
          /* local_positions */ {}));

  // Join origins.
  EXPECT_EQ(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* features */ {},
          /* local_positions */ {})
          .join(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 1,
              /* origins */ MethodSet{two},
              /* features */ {},
              /* local_positions */ {})),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ MethodSet{one, two},
          /* features */ {},
          /* local_positions */ {}));

  // Join features.
  EXPECT_EQ(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 2,
          /* origins */ {},
          /* features */
          FeatureMayAlwaysSet{context.features->get("FeatureOne")},
          /* local_positions */ {})
          .join(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 2,
              /* origins */ {},
              /* features */
              FeatureMayAlwaysSet{context.features->get("FeatureTwo")},
              /* local_positions */ {})),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 2,
          /* origins */ {},
          /* features */
          FeatureMayAlwaysSet::make_may({context.features->get("FeatureOne"),
                                         context.features->get("FeatureTwo")}),
          /* local_positions */ {}));
}

} // namespace marianatrench

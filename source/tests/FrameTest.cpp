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
      /* inferred_features */ {},
      /* locally_inferred_features */ {},
      /* user_features */ {},
      /* via_type_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {})));
  EXPECT_FALSE(Frame(
                   /* kind */ context.kinds->get("TestSource"),
                   /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                   /* callee */ nullptr,
                   /* call_position */ nullptr,
                   /* distance */ 0,
                   /* origins */ {},
                   /* inferred_features */ {},
                   /* locally_inferred_features */ {},
                   /* user_features */ {},
                   /* via_type_of_ports */ {},
                   /* local_positions */ {},
                   /* canonical_names */ {})
                   .leq(Frame::bottom()));

  // Compare kind.
  EXPECT_TRUE(Frame(
                  /* kind */ context.kinds->get("TestSource"),
                  /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                  /* callee */ nullptr,
                  /* call_position */ nullptr,
                  /* distance */ 0,
                  /* origins */ {},
                  /* inferred_features */ {},
                  /* locally_inferred_features */ {},
                  /* user_features */ {},
                  /* via_type_of_ports */ {},
                  /* local_positions */ {},
                  /* canonical_names */ {})
                  .leq(Frame(
                      /* kind */ context.kinds->get("TestSource"),
                      /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                      /* callee */ nullptr,
                      /* call_position */ nullptr,
                      /* distance */ 0,
                      /* origins */ {},
                      /* inferred_features */ {},
                      /* locally_inferred_features */ {},
                      /* user_features */ {},
                      /* via_type_of_ports */ {},
                      /* local_positions */ {},
                      /* canonical_names */ {})));
  EXPECT_FALSE(Frame(
                   /* kind */ context.kinds->get("TestSource"),
                   /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                   /* callee */ nullptr,
                   /* call_position */ nullptr,
                   /* distance */ 0,
                   /* origins */ {},
                   /* inferred_features */ {},
                   /* locally_inferred_features */ {},
                   /* user_features */ {},
                   /* via_type_of_ports */ {},
                   /* local_positions */ {},
                   /* canonical_names */ {})
                   .leq(Frame(
                       /* kind */ context.kinds->get("TestSink"),
                       /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                       /* callee */ nullptr,
                       /* call_position */ nullptr,
                       /* distance */ 0,
                       /* origins */ {},
                       /* inferred_features */ {},
                       /* locally_inferred_features */ {},
                       /* user_features */ {},
                       /* via_type_of_ports */ {},
                       /* local_positions */ {},
                       /* canonical_names */ {})));

  // Compare distances.
  EXPECT_TRUE(Frame(
                  /* kind */ context.kinds->get("TestSource"),
                  /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                  /* callee */ nullptr,
                  /* call_position */ nullptr,
                  /* distance */ 1,
                  /* origins */ {},
                  /* inferred_features */ {},
                  /* locally_inferred_features */ {},
                  /* user_features */ {},
                  /* via_type_of_ports */ {},
                  /* local_positions */ {},
                  /* canonical_names */ {})
                  .leq(Frame(
                      /* kind */ context.kinds->get("TestSource"),
                      /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                      /* callee */ nullptr,
                      /* call_position */ nullptr,
                      /* distance */ 0,
                      /* origins */ {},
                      /* inferred_features */ {},
                      /* locally_inferred_features */ {},
                      /* user_features */ {},
                      /* via_type_of_ports */ {},
                      /* local_positions */ {},
                      /* canonical_names */ {})));
  EXPECT_FALSE(Frame(
                   /* kind */ context.kinds->get("TestSource"),
                   /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                   /* callee */ nullptr,
                   /* call_position */ nullptr,
                   /* distance */ 0,
                   /* origins */ {},
                   /* inferred_features */ {},
                   /* locally_inferred_features */ {},
                   /* user_features */ {},
                   /* via_type_of_ports */ {},
                   /* local_positions */ {},
                   /* canonical_names */ {})
                   .leq(Frame(
                       /* kind */ context.kinds->get("TestSource"),
                       /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                       /* callee */ nullptr,
                       /* call_position */ nullptr,
                       /* distance */ 1,
                       /* origins */ {},
                       /* inferred_features */ {},
                       /* locally_inferred_features */ {},
                       /* user_features */ {},
                       /* via_type_of_ports */ {},
                       /* local_positions */ {},
                       /* canonical_names */ {})));

  // Compare origins.
  EXPECT_TRUE(Frame(
                  /* kind */ context.kinds->get("TestSource"),
                  /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                  /* callee */ nullptr,
                  /* call_position */ nullptr,
                  /* distance */ 0,
                  /* origins */ MethodSet{one},
                  /* inferred_features */ {},
                  /* locally_inferred_features */ {},
                  /* user_features */ {},
                  /* via_type_of_ports */ {},
                  /* local_positions */ {},
                  /* canonical_names */ {})
                  .leq(Frame(
                      /* kind */ context.kinds->get("TestSource"),
                      /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                      /* callee */ nullptr,
                      /* call_position */ nullptr,
                      /* distance */ 0,
                      /* origins */ MethodSet{one, two},
                      /* inferred_features */ {},
                      /* locally_inferred_features */ {},
                      /* user_features */ {},
                      /* via_type_of_ports */ {},
                      /* local_positions */ {},
                      /* canonical_names */ {})));
  EXPECT_FALSE(Frame(
                   /* kind */ context.kinds->get("TestSource"),
                   /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                   /* callee */ nullptr,
                   /* call_position */ nullptr,
                   /* distance */ 0,
                   /* origins */ MethodSet{one, two},
                   /* inferred_features */ {},
                   /* locally_inferred_features */ {},
                   /* user_features */ {},
                   /* via_type_of_ports */ {},
                   /* local_positions */ {},
                   /* canonical_names */ {})
                   .leq(Frame(
                       /* kind */ context.kinds->get("TestSource"),
                       /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                       /* callee */ nullptr,
                       /* call_position */ nullptr,
                       /* distance */ 0,
                       /* origins */ MethodSet{one},
                       /* inferred_features */ {},
                       /* locally_inferred_features */ {},
                       /* user_features */ {},
                       /* via_type_of_ports */ {},
                       /* local_positions */ {},
                       /* canonical_names */ {})));

  // Compare inferred features.
  EXPECT_TRUE(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */
          FeatureMayAlwaysSet::make_may({context.features->get("FeatureOne")}),
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})
          .leq(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* inferred_features */
              FeatureMayAlwaysSet::make_may(
                  {context.features->get("FeatureOne"),
                   context.features->get("FeatureTwo")}),
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {})));
  EXPECT_FALSE(Frame(
                   /* kind */ context.kinds->get("TestSource"),
                   /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                   /* callee */ nullptr,
                   /* call_position */ nullptr,
                   /* distance */ 0,
                   /* origins */ {},
                   /* inferred_features */
                   FeatureMayAlwaysSet::make_may(
                       {context.features->get("FeatureOne"),
                        context.features->get("FeatureTwo")}),
                   /* locally_inferred_features */ {},
                   /* user_features */ {},
                   /* via_type_of_ports */ {},
                   /* local_positions */ {},
                   /* canonical_names */ {})
                   .leq(Frame(
                       /* kind */ context.kinds->get("TestSource"),
                       /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                       /* callee */ nullptr,
                       /* call_position */ nullptr,
                       /* distance */ 0,
                       /* origins */ {},
                       /* inferred_features */
                       FeatureMayAlwaysSet::make_may(
                           {context.features->get("FeatureOne")}),
                       /* locally_inferred_features */ {},
                       /* user_features */ {},
                       /* via_type_of_ports */ {},
                       /* local_positions */ {},
                       /* canonical_names */ {})));

  // Compare user features.
  EXPECT_TRUE(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ FeatureSet{context.features->get("FeatureOne")},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})
          .leq(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              FeatureSet{
                  context.features->get("FeatureOne"),
                  context.features->get("FeatureTwo")},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {})));
  EXPECT_FALSE(Frame(
                   /* kind */ context.kinds->get("TestSource"),
                   /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                   /* callee */ nullptr,
                   /* call_position */ nullptr,
                   /* distance */ 0,
                   /* origins */ {},
                   /* inferred_features */ {},
                   /* locally_inferred_features */ {},
                   /* user_features */
                   FeatureSet{
                       context.features->get("FeatureOne"),
                       context.features->get("FeatureTwo")},
                   /* via_type_of_ports */ {},
                   /* local_positions */ {},
                   /* canonical_names */ {})
                   .leq(Frame(
                       /* kind */ context.kinds->get("TestSource"),
                       /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                       /* callee */ nullptr,
                       /* call_position */ nullptr,
                       /* distance */ 0,
                       /* origins */ {},
                       /* inferred_features */ {},
                       /* locally_inferred_features */ {},
                       /* user_features */
                       FeatureSet{context.features->get("FeatureOne")},
                       /* via_type_of_ports */ {},
                       /* local_positions */ {},
                       /* canonical_names */ {})));
  // Compare via_type_of_ports
  EXPECT_TRUE(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */
          RootSetAbstractDomain({Root(Root::Kind::Return)}),
          /* local_positions */ {},
          /* canonical_names */ {})
          .leq(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */
              RootSetAbstractDomain(
                  {Root(Root::Kind::Return), Root(Root::Kind::Argument, 1)}),
              /* local_positions */ {},
              /* canonical_names */ {})));
  EXPECT_FALSE(Frame(
                   /* kind */ context.kinds->get("TestSource"),
                   /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                   /* callee */ nullptr,
                   /* call_position */ nullptr,
                   /* distance */ 0,
                   /* origins */ {},
                   /* inferred_features */ {},
                   /* locally_inferred_features */ {},
                   /* user_features */ {},
                   /* via_type_of_ports */
                   RootSetAbstractDomain({Root(Root::Kind::Return)}),
                   /* local_positions */ {},
                   /* canonical_names */ {})
                   .leq(Frame(
                       /* kind */ context.kinds->get("TestSource"),
                       /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                       /* callee */ nullptr,
                       /* call_position */ nullptr,
                       /* distance */ 0,
                       /* origins */ {},
                       /* inferred_features */ {},
                       /* locally_inferred_features */ {},
                       /* user_features */ {},
                       /* via_type_of_ports */
                       RootSetAbstractDomain({Root(Root::Kind::Argument, 1)}),
                       /* local_positions */ {},
                       /* canonical_names */ {})));
  // callee_port, callee and call_position must be equal for non-artificial
  // taint.
  EXPECT_TRUE(Frame(
                  /* kind */ context.kinds->get("TestSource"),
                  /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                  /* callee */ one,
                  /* call_position */ context.positions->unknown(),
                  /* distance */ 1,
                  /* origins */ MethodSet{one},
                  /* inferred_features */ {},
                  /* locally_inferred_features */ {},
                  /* user_features */ {},
                  /* via_type_of_ports */ {},
                  /* local_positions */ {},
                  /* canonical_names */ {})
                  .leq(Frame(
                      /* kind */ context.kinds->get("TestSource"),
                      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                      /* callee */ one,
                      /* call_position */ context.positions->unknown(),
                      /* distance */ 1,
                      /* origins */ MethodSet{one},
                      /* inferred_features */ {},
                      /* locally_inferred_features */ {},
                      /* user_features */ {},
                      /* via_type_of_ports */ {},
                      /* local_positions */ {},
                      /* canonical_names */ {})));
  EXPECT_FALSE(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})
          .leq(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 1,
              /* origins */ MethodSet{one},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {})));
  EXPECT_FALSE(Frame(
                   /* kind */ context.kinds->get("TestSource"),
                   /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                   /* callee */ one,
                   /* call_position */ context.positions->unknown(),
                   /* distance */ 1,
                   /* origins */ MethodSet{one},
                   /* inferred_features */ {},
                   /* locally_inferred_features */ {},
                   /* user_features */ {},
                   /* via_type_of_ports */ {},
                   /* local_positions */ {},
                   /* canonical_names */ {})
                   .leq(Frame(
                       /* kind */ context.kinds->get("TestSource"),
                       /* callee_port */ AccessPath(Root(Root::Kind::Return)),
                       /* callee */ two,
                       /* call_position */ context.positions->unknown(),
                       /* distance */ 1,
                       /* origins */ MethodSet{one},
                       /* inferred_features */ {},
                       /* locally_inferred_features */ {},
                       /* user_features */ {},
                       /* via_type_of_ports */ {},
                       /* local_positions */ {},
                       /* canonical_names */ {})));
  EXPECT_FALSE(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})
          .leq(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->get("Test.java", 1),
              /* distance */ 1,
              /* origins */ MethodSet{one},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {})));

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
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})
          .leq(Frame(
              /* kind */ Kinds::artificial_source(),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {})));
  EXPECT_FALSE(Frame(
                   /* kind */ Kinds::artificial_source(),
                   /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
                   /* callee */ nullptr,
                   /* call_position */ nullptr,
                   /* distance */ 0,
                   /* origins */ {},
                   /* inferred_features */ {},
                   /* locally_inferred_features */ {},
                   /* user_features */ {},
                   /* via_type_of_ports */ {},
                   /* local_positions */ {},
                   /* canonical_names */ {})
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
                       /* inferred_features */ {},
                       /* locally_inferred_features */ {},
                       /* user_features */ {},
                       /* via_type_of_ports */ {},
                       /* local_positions */ {},
                       /* canonical_names */ {})));

  // Compare canonical names.
  EXPECT_TRUE(Frame(
                  /* kind */ context.kinds->get("TestSource"),
                  /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                  /* callee */ nullptr,
                  /* call_position */ nullptr,
                  /* distance */ 0,
                  /* origins */ {},
                  /* inferred_features */ {},
                  /* locally_inferred_features */ {},
                  /* user_features */ {},
                  /* via_type_of_ports */ {},
                  /* local_positions */ {},
                  /* canonical_names */ {})
                  .leq(Frame(
                      /* kind */ context.kinds->get("TestSource"),
                      /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                      /* callee */ nullptr,
                      /* call_position */ nullptr,
                      /* distance */ 0,
                      /* origins */ {},
                      /* inferred_features */ {},
                      /* locally_inferred_features */ {},
                      /* user_features */ {},
                      /* via_type_of_ports */ {},
                      /* local_positions */ {},
                      /* canonical_names */
                      CanonicalNameSetAbstractDomain{
                          CanonicalName("%programmatic_leaf_name%")})));
  EXPECT_FALSE(Frame(
                   /* kind */ context.kinds->get("TestSource"),
                   /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                   /* callee */ nullptr,
                   /* call_position */ nullptr,
                   /* distance */ 0,
                   /* origins */ {},
                   /* inferred_features */ {},
                   /* locally_inferred_features */ {},
                   /* user_features */ {},
                   /* via_type_of_ports */ {},
                   /* local_positions */ {},
                   /* canonical_names */
                   CanonicalNameSetAbstractDomain{
                       CanonicalName("%programmatic_leaf_name%")})
                   .leq(Frame(
                       /* kind */ context.kinds->get("TestSource"),
                       /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                       /* callee */ nullptr,
                       /* call_position */ nullptr,
                       /* distance */ 1,
                       /* origins */ {},
                       /* inferred_features */ {},
                       /* locally_inferred_features */ {},
                       /* user_features */ {},
                       /* via_type_of_ports */ {},
                       /* local_positions */ {},
                       /* canonical_names */ {})));
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
      /* inferred_features */ {},
      /* locally_inferred_features */ {},
      /* user_features */ {},
      /* via_type_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {})));
  EXPECT_FALSE(Frame(
                   /* kind */ context.kinds->get("TestSource"),
                   /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
                   /* callee */ nullptr,
                   /* call_position */ nullptr,
                   /* distance */ 0,
                   /* origins */ {},
                   /* inferred_features */ {},
                   /* locally_inferred_features */ {},
                   /* user_features */ {},
                   /* via_type_of_ports */ {},
                   /* local_positions */ {},
                   /* canonical_names */ {})
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
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}));
  EXPECT_EQ(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})
          .join(Frame::bottom()),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}));

  // Test incompatible joins.
  EXPECT_THROW(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})
          .join_with(Frame(
              /* kind */ context.kinds->get("TestSink"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {})),
      std::exception);
  EXPECT_THROW(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})
          .join_with(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Anchor)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {})),
      std::exception);
  EXPECT_THROW(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ {},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})
          .join_with(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ two,
              /* call_position */ context.positions->unknown(),
              /* distance */ 1,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {})),
      std::exception);
  EXPECT_THROW(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ {},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})
          .join_with(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->get("Test.java", 1),
              /* distance */ 1,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {})),
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
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})
          .join(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 1,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {})),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ {},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}));

  // Join origins.
  EXPECT_EQ(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})
          .join(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 1,
              /* origins */ MethodSet{two},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {})),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 1,
          /* origins */ MethodSet{one, two},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}));

  // Join inferred features.
  EXPECT_EQ(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 2,
          /* origins */ {},
          /* inferred_features */
          FeatureMayAlwaysSet{context.features->get("FeatureOne")},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})
          .join(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 2,
              /* origins */ {},
              /* inferred_features */
              FeatureMayAlwaysSet{context.features->get("FeatureTwo")},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {})),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 2,
          /* origins */ {},
          /* inferred_features */
          FeatureMayAlwaysSet::make_may(
              {context.features->get("FeatureOne"),
               context.features->get("FeatureTwo")}),
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}));

  // Join user features.
  EXPECT_EQ(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 2,
          /* origins */ {},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */
          FeatureSet{context.features->get("FeatureOne")},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})
          .join(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 2,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */
              FeatureSet{context.features->get("FeatureTwo")},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {})),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 2,
          /* origins */ {},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */
          FeatureSet{
              context.features->get("FeatureOne"),
              context.features->get("FeatureTwo")},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}));

  // Join via_type_of_ports
  EXPECT_EQ(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 2,
          /* origins */ {},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */
          RootSetAbstractDomain({Root(Root::Kind::Return)}),
          /* local_positions */ {},
          /* canonical_names */ {})
          .join(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 2,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */
              RootSetAbstractDomain({Root(Root::Kind::Argument, 1)}),
              /* local_positions */ {},
              /* canonical_names */ {})),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Return)),
          /* callee */ one,
          /* call_position */ context.positions->unknown(),
          /* distance */ 2,
          /* origins */ {},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */
          RootSetAbstractDomain(
              {Root(Root::Kind::Return), Root(Root::Kind::Argument, 1)}),
          /* local_positions */ {},
          /* canonical_names */ {}));

  // Join canonical names.
  EXPECT_EQ(
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ context.positions->unknown(),
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */
          CanonicalNameSetAbstractDomain{
              CanonicalName("%programmatic_leaf_name%")})
          .join(Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ context.positions->unknown(),
              /* distance */ 0,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */
              CanonicalNameSetAbstractDomain{CanonicalName("%via_type_of%")})),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ context.positions->unknown(),
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */
          CanonicalNameSetAbstractDomain{
              CanonicalName("%programmatic_leaf_name%"),
              CanonicalName("%via_type_of%")}));
}

TEST_F(FrameTest, FrameWithKind) {
  auto context = test::make_empty_context();

  auto kind_a = context.kinds->get("TestSourceA");
  auto kind_b = context.kinds->get("TestSourceB");

  Scope scope;
  auto* one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));
  auto* two = context.methods->create(
      redex::create_void_method(scope, "LOther;", "two"));

  Frame frame1(
      /* kind */ kind_a,
      /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
      /* callee */ one,
      /* call_position */ context.positions->unknown(),
      /* distance */ 5,
      /* origins */ MethodSet{two},
      /* inferred_features */
      FeatureMayAlwaysSet::make_may(
          {context.features->get("FeatureOne"),
           context.features->get("FeatureTwo")}),
      /* locally_inferred_features */ {},
      /* user_features */ {},
      /* via_type_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {});

  auto frame2 = frame1.with_kind(kind_b);
  EXPECT_EQ(frame1.callee(), frame2.callee());
  EXPECT_EQ(frame1.callee_port(), frame2.callee_port());
  EXPECT_EQ(frame1.call_position(), frame2.call_position());
  EXPECT_EQ(frame1.distance(), frame2.distance());
  EXPECT_EQ(frame1.origins(), frame2.origins());
  EXPECT_EQ(frame1.inferred_features(), frame2.inferred_features());
  EXPECT_EQ(
      frame1.locally_inferred_features(), frame2.locally_inferred_features());

  EXPECT_NE(frame1.kind(), frame2.kind());
  EXPECT_EQ(frame1.kind(), kind_a);
  EXPECT_EQ(frame2.kind(), kind_b);
}

} // namespace marianatrench

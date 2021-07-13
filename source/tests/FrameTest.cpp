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
  EXPECT_TRUE(Frame::bottom().leq(test::make_frame(
      /* kind */ context.kinds->get("TestSource"), test::FrameProperties{})));
  EXPECT_FALSE(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"), test::FrameProperties{})
          .leq(Frame::bottom()));

  // Compare kind.
  EXPECT_TRUE(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"), test::FrameProperties{})
          .leq(test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{})));
  EXPECT_FALSE(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"), test::FrameProperties{})
          .leq(test::make_frame(
              /* kind */ context.kinds->get("TestSink"),
              test::FrameProperties{})));

  // Compare distances.
  EXPECT_TRUE(test::make_frame(
                  /* kind */ context.kinds->get("TestSource"),
                  test::FrameProperties{.distance = 1})
                  .leq(test::make_frame(
                      /* kind */ context.kinds->get("TestSource"),
                      test::FrameProperties{.distance = 0})));
  EXPECT_FALSE(test::make_frame(
                   /* kind */ context.kinds->get("TestSource"),
                   test::FrameProperties{.distance = 0})
                   .leq(test::make_frame(
                       /* kind */ context.kinds->get("TestSource"),
                       test::FrameProperties{.distance = 1})));

  // Compare origins.
  EXPECT_TRUE(test::make_frame(
                  /* kind */ context.kinds->get("TestSource"),
                  test::FrameProperties{.origins = MethodSet{one}})
                  .leq(test::make_frame(
                      /* kind */ context.kinds->get("TestSource"),
                      test::FrameProperties{.origins = MethodSet{one, two}})));
  EXPECT_FALSE(test::make_frame(
                   /* kind */ context.kinds->get("TestSource"),
                   test::FrameProperties{.origins = MethodSet{one, two}})
                   .leq(test::make_frame(
                       /* kind */ context.kinds->get("TestSource"),
                       test::FrameProperties{.origins = MethodSet{one}})));

  // Compare inferred features.
  EXPECT_TRUE(test::make_frame(
                  /* kind */ context.kinds->get("TestSource"),
                  test::FrameProperties{
                      .inferred_features = FeatureMayAlwaysSet::make_may(
                          {context.features->get("FeatureOne")})})
                  .leq(test::make_frame(
                      /* kind */ context.kinds->get("TestSource"),
                      test::FrameProperties{
                          .inferred_features = FeatureMayAlwaysSet::make_may(
                              {context.features->get("FeatureOne"),
                               context.features->get("FeatureTwo")})})));
  EXPECT_FALSE(test::make_frame(
                   /* kind */ context.kinds->get("TestSource"),
                   test::FrameProperties{
                       .inferred_features = FeatureMayAlwaysSet::make_may(
                           {context.features->get("FeatureOne"),
                            context.features->get("FeatureTwo")})})
                   .leq(test::make_frame(
                       /* kind */ context.kinds->get("TestSource"),
                       test::FrameProperties{
                           .inferred_features = FeatureMayAlwaysSet::make_may(
                               {context.features->get("FeatureOne")})})));

  // Compare user features.
  EXPECT_TRUE(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .user_features = FeatureSet{context.features->get("FeatureOne")}})
          .leq(test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .locally_inferred_features = {},
                  FeatureSet{
                      context.features->get("FeatureOne"),
                      context.features->get("FeatureTwo")}})));
  EXPECT_FALSE(test::make_frame(
                   /* kind */ context.kinds->get("TestSource"),
                   test::FrameProperties{
                       .user_features =
                           FeatureSet{
                               context.features->get("FeatureOne"),
                               context.features->get("FeatureTwo")}})
                   .leq(test::make_frame(
                       /* kind */ context.kinds->get("TestSource"),
                       test::FrameProperties{
                           .user_features = FeatureSet{
                               context.features->get("FeatureOne")}})));

  // Compare via_type_of_ports
  EXPECT_TRUE(test::make_frame(
                  /* kind */ context.kinds->get("TestSource"),
                  test::FrameProperties{
                      .via_type_of_ports =
                          RootSetAbstractDomain({Root(Root::Kind::Return)})})
                  .leq(test::make_frame(
                      /* kind */ context.kinds->get("TestSource"),
                      test::FrameProperties{
                          .via_type_of_ports = RootSetAbstractDomain(
                              {Root(Root::Kind::Return),
                               Root(Root::Kind::Argument, 1)})})));
  EXPECT_FALSE(test::make_frame(
                   /* kind */ context.kinds->get("TestSource"),
                   test::FrameProperties{
                       .via_type_of_ports =
                           RootSetAbstractDomain({Root(Root::Kind::Return)})})
                   .leq(test::make_frame(
                       /* kind */ context.kinds->get("TestSource"),
                       test::FrameProperties{
                           .via_type_of_ports = RootSetAbstractDomain(
                               {Root(Root::Kind::Argument, 1)})})));

  // callee_port, callee and call_position must be equal for non-artificial
  // taint.
  EXPECT_TRUE(test::make_frame(
                  /* kind */ context.kinds->get("TestSource"),
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Return)),
                      .callee = one,
                      .call_position = context.positions->unknown(),
                      .distance = 1,
                      .origins = MethodSet{one}})
                  .leq(test::make_frame(
                      /* kind */ context.kinds->get("TestSource"),
                      test::FrameProperties{
                          .callee_port = AccessPath(Root(Root::Kind::Return)),
                          .callee = one,
                          .call_position = context.positions->unknown(),
                          .distance = 1,
                          .origins = MethodSet{one}})));
  EXPECT_FALSE(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = one,
              .call_position = context.positions->unknown(),
              .distance = 1,
              .origins = MethodSet{one}})
          .leq(test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = one,
                  .call_position = context.positions->unknown(),
                  .distance = 1,
                  .origins = MethodSet{one}})));
  EXPECT_FALSE(test::make_frame(
                   /* kind */ context.kinds->get("TestSource"),
                   test::FrameProperties{
                       .callee_port = AccessPath(Root(Root::Kind::Return)),
                       .callee = one,
                       .call_position = context.positions->unknown(),
                       .distance = 1,
                       .origins = MethodSet{one}})
                   .leq(test::make_frame(
                       /* kind */ context.kinds->get("TestSource"),
                       test::FrameProperties{
                           .callee_port = AccessPath(Root(Root::Kind::Return)),
                           .callee = two,
                           .call_position = context.positions->unknown(),
                           .distance = 1,
                           .origins = MethodSet{one}})));
  EXPECT_FALSE(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = one,
              .call_position = context.positions->unknown(),
              .distance = 1,
              .origins = MethodSet{one}})
          .leq(test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = one,
                  .call_position = context.positions->get("Test.java", 1),
                  .distance = 1,
                  .origins = MethodSet{one}})));

  // For artificial sources, compare the common prefix of callee ports.
  EXPECT_TRUE(
      test::make_frame(
          /* kind */ Kinds::artificial_source(),
          test::FrameProperties{
              .callee_port = AccessPath(
                  Root(Root::Kind::Argument, 0),
                  Path{DexString::make_string("x")})})
          .leq(test::make_frame(
              /* kind */ Kinds::artificial_source(),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})));
  EXPECT_FALSE(
      test::make_frame(
          /* kind */ Kinds::artificial_source(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})
          .leq(test::make_frame(
              /* kind */ Kinds::artificial_source(),
              test::FrameProperties{
                  .callee_port = AccessPath(
                      Root(Root::Kind::Argument, 0),
                      Path{DexString::make_string("x")})})));

  // Compare canonical names.
  EXPECT_TRUE(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"), test::FrameProperties{})
          .leq(test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .canonical_names = CanonicalNameSetAbstractDomain{
                      CanonicalName(CanonicalName::TemplateValue{
                          "%programmatic_leaf_name%"})}})));
  EXPECT_FALSE(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"%programmatic_leaf_name%"})}})
          .leq(test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{})));
}

TEST_F(FrameTest, FrameEquals) {
  auto context = test::make_empty_context();

  EXPECT_TRUE(Frame::bottom().equals(Frame::bottom()));
  EXPECT_FALSE(Frame::bottom().equals(test::make_frame(
      /* kind */ context.kinds->get("TestSource"), test::FrameProperties{})));

  EXPECT_FALSE(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"), test::FrameProperties{})
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
      Frame::bottom().join(test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{})),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{}));
  EXPECT_EQ(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"), test::FrameProperties{})
          .join(Frame::bottom()),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{}));

  // Test incompatible joins.
  EXPECT_THROW(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"), test::FrameProperties{})
          .join_with(test::make_frame(
              /* kind */ context.kinds->get("TestSink"),
              test::FrameProperties{})),
      std::exception);
  EXPECT_THROW(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Leaf))})
          .join_with(test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Anchor))})),
      std::exception);
  EXPECT_THROW(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = one,
              .call_position = context.positions->unknown(),
              .distance = 1})
          .join_with(test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = two,
                  .call_position = context.positions->unknown(),
                  .distance = 1})),
      std::exception);
  EXPECT_THROW(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = one,
              .call_position = context.positions->unknown(),
              .distance = 1})
          .join_with(test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = one,
                  .call_position = context.positions->get("Test.java", 1),
                  .distance = 1})),
      std::exception);

  // Minimum distance.
  EXPECT_EQ(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = one,
              .call_position = context.positions->unknown(),
              .distance = 2})
          .join(test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = one,
                  .call_position = context.positions->unknown(),
                  .distance = 1})),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = one,
              .call_position = context.positions->unknown(),
              .distance = 1}));

  // Join origins.
  EXPECT_EQ(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = one,
              .call_position = context.positions->unknown(),
              .distance = 1,
              .origins = MethodSet{one}})
          .join(test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = one,
                  .call_position = context.positions->unknown(),
                  .distance = 1,
                  .origins = MethodSet{two}})),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = one,
              .call_position = context.positions->unknown(),
              .distance = 1,
              .origins = MethodSet{one, two}}));

  // Join inferred features.
  EXPECT_EQ(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = one,
              .call_position = context.positions->unknown(),
              .distance = 2,
              .inferred_features =
                  FeatureMayAlwaysSet{context.features->get("FeatureOne")}})
          .join(test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = one,
                  .call_position = context.positions->unknown(),
                  .distance = 2,
                  .inferred_features =
                      FeatureMayAlwaysSet{
                          context.features->get("FeatureTwo")}})),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = one,
              .call_position = context.positions->unknown(),
              .distance = 2,
              .inferred_features = FeatureMayAlwaysSet::make_may(
                  {context.features->get("FeatureOne"),
                   context.features->get("FeatureTwo")})}));

  // Join user features.
  EXPECT_EQ(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = one,
              .call_position = context.positions->unknown(),
              .distance = 2,
              .user_features = FeatureSet{context.features->get("FeatureOne")}})
          .join(test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = one,
                  .call_position = context.positions->unknown(),
                  .distance = 2,
                  .user_features =
                      FeatureSet{context.features->get("FeatureTwo")}})),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = one,
              .call_position = context.positions->unknown(),
              .distance = 2,
              .user_features = FeatureSet{
                  context.features->get("FeatureOne"),
                  context.features->get("FeatureTwo")}}));

  // Join via_type_of_ports
  EXPECT_EQ(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = one,
              .call_position = context.positions->unknown(),
              .distance = 2,
              .via_type_of_ports =
                  RootSetAbstractDomain({Root(Root::Kind::Return)})})
          .join(test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = one,
                  .call_position = context.positions->unknown(),
                  .distance = 2,
                  .via_type_of_ports =
                      RootSetAbstractDomain({Root(Root::Kind::Argument, 1)})})),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = one,
              .call_position = context.positions->unknown(),
              .distance = 2,
              .via_type_of_ports = RootSetAbstractDomain(
                  {Root(Root::Kind::Return), Root(Root::Kind::Argument, 1)})}));

  // Join canonical names.
  EXPECT_EQ(
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .call_position = context.positions->unknown(),
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"%programmatic_leaf_name%"})}})
          .join(test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .call_position = context.positions->unknown(),
                  .canonical_names =
                      CanonicalNameSetAbstractDomain{CanonicalName(
                          CanonicalName::TemplateValue{"%via_type_of%"})}})),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .call_position = context.positions->unknown(),
              .canonical_names = CanonicalNameSetAbstractDomain{
                  CanonicalName(
                      CanonicalName::TemplateValue{"%programmatic_leaf_name%"}),
                  CanonicalName(
                      CanonicalName::TemplateValue{"%via_type_of%"})}}));
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

  auto frame1 = test::make_frame(
      /* kind */ kind_a,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Leaf)),
          .callee = one,
          .call_position = context.positions->unknown(),
          .distance = 5,
          .origins = MethodSet{two},
          .inferred_features = FeatureMayAlwaysSet::make_may(
              {context.features->get("FeatureOne"),
               context.features->get("FeatureTwo")})});

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

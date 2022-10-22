/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/CallPositionFrames.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class CallPositionFramesTest : public test::Test {};

TEST_F(CallPositionFramesTest, Add) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));
  auto* two = context.methods->create(
      redex::create_void_method(scope, "LOther;", "two"));

  auto* source_kind_one = context.kinds->get("TestSourceOne");
  auto* source_kind_two = context.kinds->get("TestSourceTwo");
  auto* feature_one = context.features->get("FeatureOne");
  auto* feature_two = context.features->get("FeatureTwo");
  auto* user_feature_one = context.features->get("UserFeatureOne");

  CallPositionFrames frames;
  EXPECT_TRUE(frames.is_bottom());
  EXPECT_TRUE(frames.empty());
  EXPECT_EQ(frames.position(), nullptr);

  frames.add(test::make_taint_config(
      source_kind_one,
      test::FrameProperties{
          .origins = MethodSet{one},
          .inferred_features = FeatureMayAlwaysSet{feature_one}}));
  EXPECT_FALSE(frames.is_bottom());
  EXPECT_EQ(frames.position(), nullptr);
  EXPECT_EQ(
      frames,
      CallPositionFrames{test::make_taint_config(
          source_kind_one,
          test::FrameProperties{
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one}})});

  // Add frame with the same kind
  frames.add(test::make_taint_config(
      source_kind_one,
      test::FrameProperties{
          .origins = MethodSet{two},
          .inferred_features = FeatureMayAlwaysSet{feature_two},
          .user_features = FeatureSet{user_feature_one}}));
  EXPECT_EQ(
      frames,
      CallPositionFrames{test::make_taint_config(
          source_kind_one,
          test::FrameProperties{
              .origins = MethodSet{one, two},
              .inferred_features =
                  FeatureMayAlwaysSet::make_may({feature_one, feature_two}),
              .user_features = FeatureSet{user_feature_one}})});

  // Add frame with a different kind
  frames.add(test::make_taint_config(
      source_kind_two,
      test::FrameProperties{
          .origins = MethodSet{two},
          .inferred_features = FeatureMayAlwaysSet{feature_two}}));
  EXPECT_EQ(
      frames,
      (CallPositionFrames{
          test::make_taint_config(
              source_kind_one,
              test::FrameProperties{
                  .origins = MethodSet{one, two},
                  .inferred_features =
                      FeatureMayAlwaysSet::make_may({feature_one, feature_two}),
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              source_kind_two,
              test::FrameProperties{
                  .origins = MethodSet{two},
                  .inferred_features = FeatureMayAlwaysSet{feature_two}})}));

  // Add frame with a different callee port
  frames.add(test::make_taint_config(
      source_kind_two,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .origins = MethodSet{two},
          .inferred_features = FeatureMayAlwaysSet{feature_two}}));
  EXPECT_EQ(
      frames,
      (CallPositionFrames{
          test::make_taint_config(
              source_kind_one,
              test::FrameProperties{
                  .origins = MethodSet{one, two},
                  .inferred_features =
                      FeatureMayAlwaysSet::make_may({feature_one, feature_two}),
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              source_kind_two,
              test::FrameProperties{
                  .origins = MethodSet{two},
                  .inferred_features = FeatureMayAlwaysSet{feature_two}}),
          test::make_taint_config(
              source_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .origins = MethodSet{two},
                  .inferred_features = FeatureMayAlwaysSet{feature_two}})}));

  // Verify frames with non-null position
  CallPositionFrames frames_with_position;
  frames_with_position.add(test::make_taint_config(
      source_kind_one,
      test::FrameProperties{.call_position = context.positions->unknown()}));
  EXPECT_EQ(frames_with_position.position(), context.positions->unknown());
}

TEST_F(CallPositionFramesTest, Leq) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));

  auto* test_kind_one = context.kinds->get("TestSinkOne");
  auto* test_kind_two = context.kinds->get("TestSinkTwo");
  auto* test_position = context.positions->get(std::nullopt, 1);

  // Comparison to bottom
  EXPECT_TRUE(CallPositionFrames::bottom().leq(CallPositionFrames::bottom()));
  EXPECT_TRUE(CallPositionFrames::bottom().leq(CallPositionFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{})}));
  EXPECT_FALSE((CallPositionFrames{test::make_taint_config(
                    test_kind_one,
                    test::FrameProperties{.call_position = test_position})})
                   .leq(CallPositionFrames::bottom()));

  // Comparison to self
  EXPECT_TRUE(
      (CallPositionFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
               .callee = one,
               .call_position = test_position,
               .distance = 1,
               .origins = MethodSet{one}})})
          .leq(CallPositionFrames{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = one,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{one}})}));

  // Same kind, different port
  EXPECT_TRUE(
      (CallPositionFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
               .callee = one,
               .call_position = test_position,
               .distance = 1,
               .origins = MethodSet{one}})})
          .leq(CallPositionFrames{
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = one,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{one}}),
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                      .callee = one,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{one}}),
          }));
  EXPECT_FALSE(
      (CallPositionFrames{
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                   .callee = one,
                   .call_position = test_position,
                   .distance = 1,
                   .origins = MethodSet{one}}),
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                   .callee = one,
                   .call_position = test_position,
                   .distance = 1,
                   .origins = MethodSet{one}}),
       })
          .leq(CallPositionFrames{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = one,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{one}})}));

  // Different kinds
  EXPECT_TRUE(
      (CallPositionFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
               .callee = one,
               .call_position = test_position,
               .distance = 1,
               .origins = MethodSet{one}})})
          .leq(CallPositionFrames{
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = one,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{one}}),
              test::make_taint_config(
                  test_kind_two,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = one,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{one}}),
          }));
  EXPECT_FALSE(
      (CallPositionFrames{
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                   .callee = one,
                   .call_position = test_position,
                   .distance = 1,
                   .origins = MethodSet{one}}),
           test::make_taint_config(
               test_kind_two,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                   .callee = one,
                   .call_position = test_position,
                   .distance = 1,
                   .origins = MethodSet{one}})})
          .leq(CallPositionFrames{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = one,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{one}})}));

  // Different callee ports
  EXPECT_TRUE(
      (CallPositionFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})})
          .leq(CallPositionFrames{
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{
                      .callee_port =
                          AccessPath(Root(Root::Kind::Argument, 0))}),
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{
                      .callee_port =
                          AccessPath(Root(Root::Kind::Argument, 1))}),
          }));
  EXPECT_FALSE(
      (CallPositionFrames{
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0))}),
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 1))})})
          .leq(CallPositionFrames{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})}));
}

TEST_F(CallPositionFramesTest, ArtificialSourceLeq) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* test_kind_one = context.kinds->get("TestSink1");

  // callee_port must be equal for non-artificial taint kinds.
  EXPECT_TRUE(
      CallPositionFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return))})}
          .leq(CallPositionFrames{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return))})}));
  EXPECT_FALSE(
      CallPositionFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return))})}
          .leq(CallPositionFrames{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})}));
  EXPECT_FALSE(
      CallPositionFrames{test::make_taint_config(
                             test_kind_one,
                             test::FrameProperties{
                                 .callee_port = AccessPath(
                                     Root(Root::Kind::Argument, 0),
                                     Path{DexString::make_string("x")})})}
          .leq(CallPositionFrames{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})}));
  EXPECT_FALSE(CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})}
                   .leq(CallPositionFrames{test::make_taint_config(
                       test_kind_one,
                       test::FrameProperties{
                           .callee_port = AccessPath(
                               Root(Root::Kind::Argument, 0),
                               Path{DexString::make_string("x")})})}));

  // For artificial sources, compare the common prefix of callee ports.
  EXPECT_TRUE(
      CallPositionFrames{test::make_taint_config(
                             Kinds::artificial_source(),
                             test::FrameProperties{
                                 .callee_port = AccessPath(
                                     Root(Root::Kind::Argument, 0),
                                     Path{DexString::make_string("x")})})}
          .leq(CallPositionFrames{test::make_taint_config(
              Kinds::artificial_source(),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})}));
  EXPECT_FALSE(CallPositionFrames{
      test::make_taint_config(
          Kinds::artificial_source(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})}
                   .leq(CallPositionFrames{test::make_taint_config(
                       Kinds::artificial_source(),
                       test::FrameProperties{
                           .callee_port = AccessPath(
                               Root(Root::Kind::Argument, 0),
                               Path{DexString::make_string("x")})})}));
}

TEST_F(CallPositionFramesTest, Equals) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));

  auto* test_kind_one = context.kinds->get("TestSinkOne");
  auto* test_kind_two = context.kinds->get("TestSinkTwo");
  auto* test_position = context.positions->get(std::nullopt, 1);

  // Comparison to bottom
  EXPECT_TRUE(
      CallPositionFrames::bottom().equals(CallPositionFrames::bottom()));
  EXPECT_FALSE(CallPositionFrames::bottom().equals(
      CallPositionFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.call_position = test_position})}));
  EXPECT_FALSE((CallPositionFrames{test::make_taint_config(
                    test_kind_one,
                    test::FrameProperties{.call_position = test_position})})
                   .equals(CallPositionFrames::bottom()));

  // Comparison to self
  EXPECT_TRUE((CallPositionFrames{test::make_taint_config(
                   test_kind_one, test::FrameProperties{})})
                  .equals(CallPositionFrames{test::make_taint_config(
                      test_kind_one, test::FrameProperties{})}));

  // Different ports
  EXPECT_FALSE(
      (CallPositionFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
               .callee = one,
               .call_position = test_position,
               .distance = 1,
               .origins = MethodSet{one}})})
          .equals(CallPositionFrames{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .callee = one,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{one}})}));

  // Different kinds
  EXPECT_FALSE((CallPositionFrames{test::make_taint_config(
                    test_kind_one, test::FrameProperties{})})
                   .equals(CallPositionFrames{test::make_taint_config(
                       test_kind_two, test::FrameProperties{})}));
}

TEST_F(CallPositionFramesTest, JoinWith) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));

  auto* test_kind_one = context.kinds->get("TestSinkOne");
  auto* test_kind_two = context.kinds->get("TestSinkTwo");
  auto* test_position = context.positions->get(std::nullopt, 1);

  // Join with bottom
  EXPECT_EQ(
      CallPositionFrames::bottom().join(CallPositionFrames{
          test::make_taint_config(test_kind_one, test::FrameProperties{})}),
      CallPositionFrames{
          test::make_taint_config(test_kind_one, test::FrameProperties{})});

  EXPECT_EQ(
      (CallPositionFrames{
           test::make_taint_config(test_kind_one, test::FrameProperties{})})
          .join(CallPositionFrames::bottom()),
      CallPositionFrames{
          test::make_taint_config(test_kind_one, test::FrameProperties{})});

  // Join with bottom (non-null call position)
  auto frames = (CallPositionFrames{test::make_taint_config(
                     test_kind_one,
                     test::FrameProperties{.call_position = test_position})})
                    .join(CallPositionFrames::bottom());
  EXPECT_EQ(
      frames,
      CallPositionFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.call_position = test_position})});
  EXPECT_EQ(frames.position(), test_position);

  frames = CallPositionFrames::bottom().join(
      CallPositionFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.call_position = test_position})});
  EXPECT_EQ(
      frames,
      CallPositionFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.call_position = test_position})});
  EXPECT_EQ(frames.position(), test_position);

  // Join different kinds
  frames = CallPositionFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{})};
  frames.join_with(CallPositionFrames{
      test::make_taint_config(test_kind_two, test::FrameProperties{})});
  EXPECT_EQ(
      frames,
      (CallPositionFrames{
          test::make_taint_config(test_kind_one, test::FrameProperties{}),
          test::make_taint_config(test_kind_two, test::FrameProperties{})}));

  // Join same kind
  auto frame_one = test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
          .callee = one,
          .call_position = test_position,
          .distance = 1,
          .origins = MethodSet{one}});
  auto frame_two = test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
          .callee = one,
          .call_position = test_position,
          .distance = 2,
          .origins = MethodSet{one}});
  frames = CallPositionFrames{frame_one};
  frames.join_with(CallPositionFrames{frame_two});
  EXPECT_EQ(frames, (CallPositionFrames{frame_one}));

  // Join different ports
  frames = CallPositionFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})};
  frames.join_with(CallPositionFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 1))})});
  EXPECT_EQ(
      frames,
      (CallPositionFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))}),
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1))}),
      }));

  // Join same ports (different kinds)
  frames = CallPositionFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})};
  frames.join_with(CallPositionFrames{test::make_taint_config(
      test_kind_two,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})});
  EXPECT_EQ(
      frames,
      (CallPositionFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))}),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))}),
      }));
}

TEST_F(CallPositionFramesTest, ArtificialSourceJoinWith) {
  auto context = test::make_empty_context();
  auto* test_kind_one = context.kinds->get("TestSinkOne");

  // Join different ports with same prefix for artificial kinds.
  // Ports should be collapsed to the common prefix.
  auto frames = CallPositionFrames{test::make_taint_config(
      Kinds::artificial_source(),
      test::FrameProperties{
          .callee_port = AccessPath(
              Root(Root::Kind::Argument, 0),
              Path{DexString::make_string("x")})})};
  frames.join_with(CallPositionFrames{test::make_taint_config(
      Kinds::artificial_source(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})});
  EXPECT_EQ(
      frames,
      CallPositionFrames{test::make_taint_config(
          Kinds::artificial_source(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})});

  // Join different ports with same prefix, for non-artificial kinds
  frames = CallPositionFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(
              Root(Root::Kind::Argument, 0),
              Path{DexString::make_string("x")})})};
  frames.join_with(CallPositionFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})});
  EXPECT_EQ(
      frames,
      (CallPositionFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(
                      Root(Root::Kind::Argument, 0),
                      Path{DexString::make_string("x")})}),
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))}),
      }));
  EXPECT_NE(
      frames,
      (CallPositionFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(
                      Root(Root::Kind::Argument, 0),
                      Path{DexString::make_string("x")})}),
      }));
  EXPECT_NE(
      frames,
      (CallPositionFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))}),
      }));
}

TEST_F(CallPositionFramesTest, Difference) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));
  auto* three = context.methods->create(
      redex::create_void_method(scope, "LThree;", "three"));

  const auto* x = DexString::make_string("x");
  const auto* y = DexString::make_string("y");

  auto* test_kind_one = context.kinds->get("TestSinkOne");
  auto* test_kind_two = context.kinds->get("TestSinkTwo");
  auto* test_position = context.positions->get(std::nullopt, 1);
  auto* feature_one = context.features->get("FeatureOne");
  auto* feature_two = context.features->get("FeatureTwo");
  auto* user_feature_one = context.features->get("UserFeatureOne");
  auto* user_feature_two = context.features->get("UserFeatureTwo");

  CallPositionFrames frames, initial_frames;

  // Tests with empty left hand side.
  frames.difference_with(CallPositionFrames{});
  EXPECT_TRUE(frames.is_bottom());

  frames.difference_with(CallPositionFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
  });
  EXPECT_TRUE(frames.is_bottom());

  initial_frames = CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one}}),
  };

  frames = initial_frames;
  frames.difference_with(CallPositionFrames{});
  EXPECT_EQ(frames, initial_frames);

  frames = initial_frames;
  frames.difference_with(CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one}}),
  });
  EXPECT_TRUE(frames.is_bottom());

  // Left hand side is bigger than right hand side.
  frames = initial_frames;
  frames.difference_with(CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
  });
  EXPECT_EQ(frames, initial_frames);

  // Left hand side and right hand side have different inferred features.
  frames = initial_frames;
  frames.difference_with(CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_one}}),
  });
  EXPECT_EQ(frames, initial_frames);

  // Left hand side and right hand side have different user features.
  frames = initial_frames;
  frames.difference_with(CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_two}}),
  });
  EXPECT_EQ(frames, initial_frames);

  // Left hand side and right hand side have different callee_ports.
  frames = initial_frames;
  frames.difference_with(CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one}}),
  });
  EXPECT_EQ(frames, initial_frames);

  // Left hand side is smaller than right hand side (with one kind).
  frames = CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one}}),
  };
  frames.difference_with(CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one}}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two},
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_two}}),
  });
  EXPECT_TRUE(frames.is_bottom());

  // Left hand side has more kinds than right hand side.
  frames = CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
  };
  frames.difference_with(CallPositionFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
          .callee = one,
          .call_position = test_position,
          .distance = 1,
          .origins = MethodSet{one}})});
  EXPECT_EQ(
      frames,
      (CallPositionFrames{
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = one,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{one}}),
      }));

  // Left hand side is smaller for one kind, and larger for another.
  frames = CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three}}),
  };
  frames.difference_with(CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}})});
  EXPECT_EQ(
      frames,
      (CallPositionFrames{test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three}})}));

  // Both sides contain access paths
  frames = CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0), Path{y}),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
  };
  frames.difference_with(CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0), Path{y}),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three}}),
  });
  EXPECT_TRUE(frames.is_bottom());

  // Left hand side larger than right hand side for specific frames.
  frames = CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one, two}}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one, three}}),
  };
  frames.difference_with(CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one, two, three}}),
  });
  EXPECT_EQ(
      frames,
      (CallPositionFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = one,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{one, two}}),
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{two}}),
      }));
}

TEST_F(CallPositionFramesTest, Iterator) {
  auto context = test::make_empty_context();

  auto* test_kind_one = context.kinds->get("TestSinkOne");
  auto* test_kind_two = context.kinds->get("TestSinkTwo");

  auto call_position_frames = CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0))}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1))}),
      test::make_taint_config(test_kind_two, test::FrameProperties{})};

  std::vector<Frame> frames;
  for (const auto& frame : call_position_frames) {
    frames.push_back(frame);
  }

  EXPECT_EQ(frames.size(), 3);
  EXPECT_NE(
      std::find(
          frames.begin(),
          frames.end(),
          test::make_taint_frame(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})),
      frames.end());
  EXPECT_NE(
      std::find(
          frames.begin(),
          frames.end(),
          test::make_taint_frame(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1))})),
      frames.end());
  EXPECT_NE(
      std::find(
          frames.begin(),
          frames.end(),
          test::make_taint_frame(test_kind_two, test::FrameProperties{})),
      frames.end());
}

TEST_F(CallPositionFramesTest, Map) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* test_kind = context.kinds->get("TestSink");
  auto* test_position = context.positions->get(std::nullopt, 1);
  auto* feature_one = context.features->get("FeatureOne");

  auto frames = CallPositionFrames{
      test::make_taint_config(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_taint_config(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = one,
              .call_position = test_position,
              .distance = 2,
              .origins = MethodSet{one}}),
  };
  frames.map([feature_one](Frame& frame) {
    frame.add_inferred_features(FeatureMayAlwaysSet{feature_one});
  });
  EXPECT_EQ(
      frames,
      (CallPositionFrames{
          test::make_taint_config(
              test_kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = one,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{one},
                  .locally_inferred_features =
                      FeatureMayAlwaysSet{feature_one}}),
          test::make_taint_config(
              test_kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .callee = one,
                  .call_position = test_position,
                  .distance = 2,
                  .origins = MethodSet{one},
                  .locally_inferred_features =
                      FeatureMayAlwaysSet{feature_one}}),
      }));
}

TEST_F(CallPositionFramesTest, FeaturesAndPositions) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* test_kind_one = context.kinds->get("TestSinkOne");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);
  auto* feature_one = context.features->get("FeatureOne");
  auto* feature_two = context.features->get("FeatureTwo");

  // add_inferred_features should be an *add* operation on the features,
  // not a join.
  auto frames = CallPositionFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .locally_inferred_features = FeatureMayAlwaysSet(
              /* may */ FeatureSet{feature_one},
              /* always */ FeatureSet{})})};
  frames.add_inferred_features(FeatureMayAlwaysSet{feature_two});
  EXPECT_EQ(
      frames.inferred_features(),
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{feature_one},
          /* always */ FeatureSet{feature_two}));

  // Test add_local_position
  frames = CallPositionFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return))})};
  EXPECT_EQ(frames.local_positions(), LocalPositionSet{});
  frames.add_local_position(test_position_one);
  EXPECT_EQ(frames.local_positions(), LocalPositionSet{test_position_one});

  // Test local_positions() with two frames, each with different positions.
  auto frames_with_different_port = CallPositionFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})};
  frames_with_different_port.add_local_position(test_position_two);
  EXPECT_EQ(
      frames_with_different_port.local_positions(),
      LocalPositionSet{test_position_two});
  frames.join_with(frames_with_different_port);
  EXPECT_EQ(
      frames.local_positions(),
      (LocalPositionSet{test_position_one, test_position_two}));

  // Remove a frame. Verify that local_position of the other frame,
  // i.e. local positions were kept separately after a join_with.
  frames.filter_invalid_frames(
      /* is_valid */ [&](auto _callee, auto access_path, auto _kind) {
        return access_path == AccessPath(Root(Root::Kind::Return));
      });
  EXPECT_EQ(frames.local_positions(), LocalPositionSet{test_position_one});

  // Verify: add_local_position adds the position to all frames.
  frames.join_with(CallPositionFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})});
  frames.add_local_position(test_position_two);
  EXPECT_EQ(
      frames.local_positions(),
      (LocalPositionSet{test_position_one, test_position_two}));
  frames.filter_invalid_frames(
      /* is_valid */ [&](auto _callee, auto access_path, auto _kind) {
        return access_path == AccessPath(Root(Root::Kind::Return));
      });
  EXPECT_EQ(
      frames.local_positions(),
      (LocalPositionSet{test_position_one, test_position_two}));

  // Verify set_local_positions.
  frames.set_local_positions(LocalPositionSet{test_position_two});
  EXPECT_EQ(frames.local_positions(), LocalPositionSet{test_position_two});

  // Verify add_inferred_features_and_local_position.
  frames.join_with(CallPositionFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})});
  frames.add_inferred_features_and_local_position(
      /* features */ FeatureMayAlwaysSet{feature_one},
      /* position */ test_position_one);
  EXPECT_EQ(
      frames.local_positions(),
      (LocalPositionSet{test_position_one, test_position_two}));
  EXPECT_EQ(frames.inferred_features(), FeatureMayAlwaysSet{feature_one});
}

TEST_F(CallPositionFramesTest, Propagate) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto* test_kind_one = context.kinds->get("TestSinkOne");
  auto* test_kind_two = context.kinds->get("TestSinkTwo");
  auto* call_position = context.positions->get("Test.java", 1);

  // It is generally expected (though not enforced) that frames within
  // `CallPositionFrames` have the same callee because of the `Taint`
  // structure. They typically also share the same "callee_port" because they
  // share the same `Position`. However, for testing purposes, we use
  // different callees and callee ports.
  auto frames = CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.callee = two, .origins = MethodSet{two}}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Anchor)),
              .origins = MethodSet{one},
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"%programmatic_leaf_name%"})}}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{.callee = one, .origins = MethodSet{one}}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Anchor)),
              .origins = MethodSet{one},
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"%programmatic_leaf_name%"})}}),
  };

  auto expected_instantiated_name =
      CanonicalName(CanonicalName::InstantiatedValue{two->signature()});
  EXPECT_EQ(
      frames.propagate(
          /* callee */ two,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          call_position,
          /* maximum_source_sink_distance */ 100,
          context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {}),
      (CallPositionFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = call_position,
                  .distance = 1,
                  .origins = MethodSet{one, two},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom()}),
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(
                      Root(Root::Kind::Anchor),
                      Path{DexString::make_string("Argument(-1)")}),
                  .callee = two,
                  .call_position = call_position,
                  .origins = MethodSet{one},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .canonical_names =
                      CanonicalNameSetAbstractDomain{
                          expected_instantiated_name}}),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = call_position,
                  .distance = 1,
                  .origins = MethodSet{one},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom()}),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(
                      Root(Root::Kind::Anchor),
                      Path{DexString::make_string("Argument(-1)")}),
                  .callee = two,
                  .call_position = call_position,
                  .origins = MethodSet{one},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .canonical_names =
                      CanonicalNameSetAbstractDomain{
                          expected_instantiated_name}}),
      }));
}

TEST_F(CallPositionFramesTest, PropagateDropFrames) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto* test_kind_one = context.kinds->get("TestSinkOne");
  auto* test_kind_two = context.kinds->get("TestSinkTwo");
  auto* call_position = context.positions->get("Test.java", 1);
  auto* user_feature_one = context.features->get("UserFeatureOne");
  auto* user_feature_two = context.features->get("UserFeatureTwo");

  // Propagating this frame will give it a distance of 2. It is expected to be
  // dropped as it exceeds the maximum distance allowed.
  auto frames = CallPositionFrames{
      test::make_taint_config(
          test_kind_one, test::FrameProperties{.callee = one, .distance = 1}),
  };
  EXPECT_EQ(
      frames.propagate(
          /* callee */ two,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          call_position,
          /* maximum_source_sink_distance */ 1,
          context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {}),
      CallPositionFrames::bottom());

  // One of the two frames will be ignored during propagation because its
  // distance exceeds the maximum distance allowed.
  frames = CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .distance = 2,
              .user_features = FeatureSet{user_feature_one}}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee = one,
              .distance = 1,
              .user_features = FeatureSet{user_feature_two}}),
  };
  EXPECT_EQ(
      frames.propagate(
          /* callee */ two,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          call_position,
          /* maximum_source_sink_distance */ 2,
          context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {}),
      (CallPositionFrames{
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = call_position,
                  .distance = 2,
                  .inferred_features = FeatureMayAlwaysSet{user_feature_two},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom()}),
      }));
}

TEST_F(CallPositionFramesTest, PartitionMap) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* test_kind = context.kinds->get("TestSink");

  auto frames = CallPositionFrames{
      test::make_taint_config(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .origins = MethodSet{one}}),
      test::make_taint_config(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Anchor)),
              .origins = MethodSet{one},
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"%programmatic_leaf_name%"})}}),
  };

  auto partitions = frames.partition_map<bool>(
      [](const Frame& frame) { return frame.is_crtex_producer_declaration(); });

  EXPECT_EQ(partitions[true].size(), 1);
  EXPECT_EQ(
      partitions[true][0],
      test::make_taint_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Anchor)),
              .origins = MethodSet{one},
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"%programmatic_leaf_name%"})}}));

  EXPECT_EQ(partitions[false].size(), 1);
  EXPECT_EQ(
      partitions[false][0],
      test::make_taint_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .origins = MethodSet{one}}));
}

TEST_F(CallPositionFramesTest, AttachPosition) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto* feature_one = context.features->get("FeatureOne");
  auto* feature_two = context.features->get("FeatureTwo");
  auto* test_kind = context.kinds->get("TestSink");
  auto* test_position = context.positions->get(std::nullopt, 1);

  auto frames = CallPositionFrames{
      test::make_taint_config(
          test_kind,
          test::FrameProperties{
              .call_position = test_position,
              .origins = MethodSet{one},
              .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{feature_two}}),
      test::make_taint_config(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .call_position = test_position,
              .origins = MethodSet{two}}),
  };

  auto* new_test_position = context.positions->get(std::nullopt, 2);
  auto frames_with_new_position = frames.attach_position(new_test_position);

  EXPECT_EQ(frames_with_new_position.position(), new_test_position);
  EXPECT_EQ(
      frames_with_new_position,
      (CallPositionFrames{
          test::make_taint_config(
              test_kind,
              test::FrameProperties{
                  .call_position = new_test_position,
                  .origins = MethodSet{one},
                  .inferred_features =
                      FeatureMayAlwaysSet{feature_one, feature_two},
                  .locally_inferred_features =
                      FeatureMayAlwaysSet{feature_two}}),
          test::make_taint_config(
              test_kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .call_position = new_test_position,
                  .origins = MethodSet{two},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom()}),
      }));
}

TEST_F(CallPositionFramesTest, TransformKindWithFeatures) {
  auto context = test::make_empty_context();

  auto* test_position = context.positions->get(std::nullopt, 1);
  auto* feature_one = context.features->get("FeatureOne");
  auto* feature_two = context.features->get("FeatureTwo");
  auto* user_feature_one = context.features->get("UserFeatureOne");

  auto* test_kind_one = context.kinds->get("TestKindOne");
  auto* test_kind_two = context.kinds->get("TestKindTwo");
  auto* transformed_test_kind_one =
      context.kinds->get("TransformedTestKindOne");
  auto* transformed_test_kind_two =
      context.kinds->get("TransformedTestKindTwo");

  auto initial_frames = CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .call_position = test_position,
              .user_features = FeatureSet{user_feature_one}}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .call_position = test_position,
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one}}),
  };

  // Drop all kinds.
  auto empty_frames = initial_frames;
  empty_frames.transform_kind_with_features(
      [](const auto* /* unused kind */) { return std::vector<const Kind*>(); },
      [](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet::bottom();
      });
  EXPECT_EQ(empty_frames, CallPositionFrames::bottom());

  // Perform an actual transformation.
  auto new_frames = initial_frames;
  new_frames.transform_kind_with_features(
      [&](const auto* kind) -> std::vector<const Kind*> {
        if (kind == test_kind_one) {
          return {transformed_test_kind_one};
        }
        return {kind};
      },
      [](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet::bottom();
      });
  EXPECT_EQ(
      new_frames,
      (CallPositionFrames{
          test::make_taint_config(
              transformed_test_kind_one,
              test::FrameProperties{
                  .call_position = test_position,
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .call_position = test_position,
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
      }));

  // Another transformation, this time including a change to the features
  new_frames = initial_frames;
  new_frames.transform_kind_with_features(
      [&](const auto* kind) {
        if (kind == test_kind_one) {
          return std::vector<const Kind*>{transformed_test_kind_one};
        }
        return std::vector<const Kind*>{kind};
      },
      [feature_one](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet{feature_one};
      });
  EXPECT_EQ(
      new_frames,
      (CallPositionFrames{
          test::make_taint_config(
              transformed_test_kind_one,
              test::FrameProperties{
                  .call_position = test_position,
                  .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .call_position = test_position,
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
      }));

  // Tests one -> many transformations (with features).
  new_frames = initial_frames;
  new_frames.transform_kind_with_features(
      [&](const auto* kind) {
        if (kind == test_kind_one) {
          return std::vector<const Kind*>{
              test_kind_one,
              transformed_test_kind_one,
              transformed_test_kind_two};
        }
        return std::vector<const Kind*>{};
      },
      [feature_one](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet{feature_one};
      });
  EXPECT_EQ(
      new_frames,
      (CallPositionFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .call_position = test_position,
                  .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              transformed_test_kind_one,
              test::FrameProperties{
                  .call_position = test_position,
                  .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              transformed_test_kind_two,
              test::FrameProperties{
                  .call_position = test_position,
                  .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
      }));

  // Tests transformations with features added to specific kinds.
  new_frames = initial_frames;
  new_frames.transform_kind_with_features(
      [&](const auto* kind) {
        if (kind == test_kind_one) {
          return std::vector<const Kind*>{
              transformed_test_kind_one, transformed_test_kind_two};
        }
        return std::vector<const Kind*>{};
      },
      [&](const auto* transformed_kind) {
        if (transformed_kind == transformed_test_kind_one) {
          return FeatureMayAlwaysSet{feature_one};
        }
        return FeatureMayAlwaysSet::bottom();
      });
  EXPECT_EQ(
      new_frames,
      (CallPositionFrames{
          test::make_taint_config(
              transformed_test_kind_one,
              test::FrameProperties{
                  .call_position = test_position,
                  .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              transformed_test_kind_two,
              test::FrameProperties{
                  .call_position = test_position,
                  .user_features = FeatureSet{user_feature_one}}),
      }));

  // Transformation where multiple old kinds map to the same new kind
  auto frames = CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .call_position = test_position,
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_one}}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .call_position = test_position,
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one}}),
  };
  frames.transform_kind_with_features(
      [&](const auto* /* unused kind */) -> std::vector<const Kind*> {
        return {transformed_test_kind_one};
      },
      [](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet::bottom();
      });
  EXPECT_EQ(
      frames,
      (CallPositionFrames{
          test::make_taint_config(
              transformed_test_kind_one,
              test::FrameProperties{
                  .call_position = test_position,
                  .inferred_features = FeatureMayAlwaysSet(
                      /* may */ FeatureSet{feature_one, feature_two},
                      /* always */ FeatureSet{}),
                  .user_features = FeatureSet{user_feature_one}}),
      }));
}

TEST_F(CallPositionFramesTest, AppendCalleePort) {
  auto context = test::make_empty_context();

  auto* test_kind = context.kinds->get("TestKind");
  const auto* path_element1 = DexString::make_string("field1");
  const auto* path_element2 = DexString::make_string("field2");

  auto frames = CallPositionFrames{
      test::make_taint_config(test_kind, test::FrameProperties{}),
      test::make_taint_config(
          Kinds::artificial_source(),
          test::FrameProperties{
              .callee_port = AccessPath(
                  Root(Root::Kind::Argument), Path{path_element1})})};

  frames.append_callee_port_to_artificial_sources(path_element2);
  EXPECT_EQ(
      frames,
      (CallPositionFrames{
          test::make_taint_config(test_kind, test::FrameProperties{}),
          test::make_taint_config(
              Kinds::artificial_source(),
              test::FrameProperties{
                  .callee_port = AccessPath(
                      Root(Root::Kind::Argument),
                      Path{path_element1, path_element2})})}));
}

TEST_F(CallPositionFramesTest, MapPositions) {
  auto context = test::make_empty_context();

  auto* test_kind_one = context.kinds->get("TestKind1");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);

  // Verify bottom() maps to nothing (empty map).
  auto frames = CallPositionFrames::bottom();
  auto new_positions = frames.map_positions(
      /* new_call_position */ [&](const auto& _access_path,
                                  const auto* position) { return position; },
      /* new_local_positions */
      [&](const auto& local_positions) { return local_positions; });
  EXPECT_EQ(new_positions.size(), 0);

  // Verify call position mapping with possibly multiple frames mapping to
  // the same output call position.
  frames = CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .call_position = test_position_one,
          }),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .call_position = test_position_one,
          }),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .call_position = test_position_one,
          }),
  };
  new_positions = frames.map_positions(
      /* new_call_position */
      [&](const auto& access_path, const auto* position) {
        if (access_path.root().is_return()) {
          return context.positions->get(
              position, position->line(), /* start */ 1, /* end */ 1);
        } else {
          return context.positions->get(
              position, position->line(), /* start */ 2, /* end */ 2);
        }
      },
      /* new_local_positions */
      [&](const auto& local_positions) { return local_positions; });
  const auto* expected_return_position = context.positions->get(
      /* path */ std::nullopt,
      /* line */ test_position_one->line(),
      /* port */ {},
      /* instruction */ nullptr,
      /* start */ 1,
      /* end */ 1);
  const auto* expected_argument_position = context.positions->get(
      /* path */ std::nullopt,
      /* line */ test_position_one->line(),
      /* port */ {},
      /* instruction */ nullptr,
      /* start */ 2,
      /* end */ 2);
  EXPECT_EQ(new_positions.size(), 2);
  EXPECT_EQ(
      new_positions[expected_return_position],
      CallPositionFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .call_position = expected_return_position,
          })});
  EXPECT_EQ(
      new_positions[expected_argument_position],
      (CallPositionFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .call_position = expected_argument_position,
              }),
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .call_position = expected_argument_position,
              })}));

  // Verify local position mapping
  frames = CallPositionFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .call_position = test_position_one,
          }),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .call_position = test_position_one,
          }),
  };
  frames.set_local_positions(LocalPositionSet{test_position_two});
  new_positions = frames.map_positions(
      /* new_call_position */ [&](const auto& _access_path,
                                  const auto* position) { return position; },
      /* new_local_positiions */
      [&](const auto& local_positions) {
        auto new_local_positions = LocalPositionSet{};
        for (const auto* position : local_positions.elements()) {
          new_local_positions.add(context.positions->get(
              position,
              position->line(),
              /* start */ 3,
              /* end */ 3));
        }
        return new_local_positions;
      });
  const auto* expected_local_position = context.positions->get(
      /* path */ std::nullopt,
      /* line */ test_position_two->line(),
      /* port */ {},
      /* instruction */ nullptr,
      /* start */ 3,
      /* end */ 3);
  auto expected_frames = frames;
  expected_frames.set_local_positions(
      LocalPositionSet{expected_local_position});
  EXPECT_EQ(new_positions.size(), 1);
  EXPECT_EQ(new_positions[test_position_one], expected_frames);
}

TEST_F(CallPositionFramesTest, FilterInvalidFrames) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method1 =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* test_kind_one = context.kinds->get("TestSourceOne");
  auto* test_kind_two = context.kinds->get("TestSourceTwo");

  // Filter by callee
  auto frames = CallPositionFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .callee = method1})};
  frames.filter_invalid_frames(
      /* is_valid */
      [&](const Method* MT_NULLABLE callee,
          const AccessPath& /* callee_port */,
          const Kind* /* kind */) { return callee == nullptr; });
  EXPECT_EQ(
      frames,
      (CallPositionFrames{
          test::make_taint_config(test_kind_one, test::FrameProperties{})}));

  // Filter by callee port
  frames = CallPositionFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .callee = method1})};
  frames.filter_invalid_frames(
      /* is_valid */
      [&](const Method* MT_NULLABLE /* callee */,
          const AccessPath& callee_port,
          const Kind* /* kind */) {
        return callee_port == AccessPath(Root(Root::Kind::Argument));
      });
  EXPECT_EQ(
      frames,
      (CallPositionFrames{test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .callee = method1})}));

  // Filter by kind
  frames = CallPositionFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .callee = method1})};
  frames.filter_invalid_frames(
      /* is_valid */
      [&](const Method* MT_NULLABLE /* callee */,
          const AccessPath& /* callee_port */,
          const Kind* kind) { return kind != test_kind_two; });
  EXPECT_EQ(
      frames,
      (CallPositionFrames{
          test::make_taint_config(test_kind_one, test::FrameProperties{})}));
}

TEST_F(CallPositionFramesTest, Show) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* test_kind_one = context.kinds->get("TestSink1");
  auto frame_one = test::make_taint_config(
      test_kind_one, test::FrameProperties{.origins = MethodSet{one}});
  auto frames = CallPositionFrames{frame_one};

  EXPECT_EQ(
      show(frames),
      "[FramesByCalleePort(CalleePortFrames(callee_port=AccessPath(Leaf), "
      "is_artificial_source_frames=0, frames=[FrameByKind(kind=TestSink1, "
      "frames={Frame(kind=`TestSink1`, callee_port=AccessPath(Leaf), "
      "origins={`LOne;.one:()V`})}),])),]");

  EXPECT_EQ(show(CallPositionFrames::bottom()), "[]");
}

TEST_F(CallPositionFramesTest, ContainsKind) {
  auto context = test::make_empty_context();

  auto frames = CallPositionFrames{
      test::make_taint_config(
          /* kind */ context.kinds->get("TestSource"), test::FrameProperties{}),
      test::make_taint_config(
          Kinds::artificial_source(), test::FrameProperties{})};

  EXPECT_TRUE(frames.contains_kind(Kinds::artificial_source()));
  EXPECT_TRUE(frames.contains_kind(context.kinds->get("TestSource")));
  EXPECT_FALSE(frames.contains_kind(context.kinds->get("TestSink")));
}

TEST_F(CallPositionFramesTest, PartitionByKind) {
  auto context = test::make_empty_context();

  auto* test_position = context.positions->get(std::nullopt, 1);
  auto* test_kind_one = context.kinds->get("TestSource1");
  auto* test_kind_two = context.kinds->get("TestSource2");

  auto frames = CallPositionFrames{
      test::make_taint_config(
          test_kind_one, test::FrameProperties{.call_position = test_position}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .call_position = test_position}),
      test::make_taint_config(
          test_kind_two, test::FrameProperties{.call_position = test_position}),
  };

  auto frames_by_kind = frames.partition_by_kind<const Kind*>(
      [](const Kind* kind) { return kind; });
  EXPECT_TRUE(frames_by_kind.size() == 2);
  EXPECT_EQ(
      frames_by_kind[test_kind_one],
      (CallPositionFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{.call_position = test_position}),
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .call_position = test_position}),
      }));
  EXPECT_EQ(frames_by_kind[test_kind_one].position(), test_position);
  EXPECT_EQ(
      frames_by_kind[test_kind_two],
      CallPositionFrames{test::make_taint_config(
          test_kind_two,
          test::FrameProperties{.call_position = test_position})});
  EXPECT_EQ(frames_by_kind[test_kind_two].position(), test_position);
}

} // namespace marianatrench

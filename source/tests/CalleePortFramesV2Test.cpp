/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/CalleePortFramesV2.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class CalleePortFramesV2Test : public test::Test {};

TEST_F(CalleePortFramesV2Test, Constructor) {
  auto context = test::make_empty_context();

  auto* test_kind_one = context.kinds->get("TestSinkOne");
  auto* test_kind_two = context.kinds->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);
  const auto* x = DexString::make_string("x");
  const auto* y = DexString::make_string("y");
  const auto* z = DexString::make_string("z");

  // Verify local positions only need to be specified on one TaintBuilder in
  // order to apply to the whole object.
  auto frames = CalleePortFramesV2{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .local_positions = LocalPositionSet{test_position_one}}),
      test::make_taint_config(test_kind_two, test::FrameProperties{})};
  EXPECT_EQ(frames.local_positions(), LocalPositionSet{test_position_one});

  // Specifying the same position on both builders should have the same result.
  EXPECT_EQ(
      frames,
      (CalleePortFramesV2{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .local_positions = LocalPositionSet{test_position_one}}),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .local_positions = LocalPositionSet{test_position_one}})}));

  // Specifying different local positions would result in them being joined and
  // applied to all frames.
  frames = CalleePortFramesV2{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .local_positions = LocalPositionSet{test_position_one}}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .local_positions = LocalPositionSet{test_position_two}})};
  EXPECT_EQ(
      frames.local_positions(),
      (LocalPositionSet{test_position_one, test_position_two}));

  // Default constructed and real sources have output paths set to bottom.
  EXPECT_EQ(frames.output_paths(), PathTreeDomain::bottom());
  frames = CalleePortFramesV2();
  EXPECT_EQ(frames.output_paths(), PathTreeDomain::bottom());

  // Specifying different output paths would result in them being joined and
  // applied to all frames.
  frames = CalleePortFramesV2(
      {test::make_taint_config(
           Kinds::receiver(),
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
               .output_paths =
                   PathTreeDomain{{Path{x, y}, SingletonAbstractDomain()}}}),
       test::make_taint_config(
           Kinds::receiver(),
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
               .output_paths =
                   PathTreeDomain{{Path{x, z}, SingletonAbstractDomain()}}})});
  EXPECT_EQ(
      frames.output_paths(),
      PathTreeDomain(
          {{Path{x, y}, SingletonAbstractDomain()},
           {Path{x, z}, SingletonAbstractDomain()}}));

  // (This tests TaintConfig more so than CalleePortFrames) Ensure that local
  // result and receiver taints are places together.
  EXPECT_THROW(
      CalleePortFramesV2({test::make_taint_config(
          Kinds::local_result(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .output_paths =
                  PathTreeDomain{{Path{x, y}, SingletonAbstractDomain()}}})}),
      std::exception);

  // Can't construct from different kinds of result/receiver sinks.
  EXPECT_THROW(
      CalleePortFramesV2(
          {test::make_taint_config(
               Kinds::receiver(),
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0))}),
           test::make_taint_config(
               Kinds::local_result(),
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Return))})}),
      std::exception);
}

TEST_F(CalleePortFramesV2Test, Add) {
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

  CalleePortFramesV2 frames;
  EXPECT_TRUE(frames.is_bottom());
  EXPECT_TRUE(frames.empty());

  frames.add(test::make_taint_config(
      source_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .origins = MethodSet{one},
          .inferred_features = FeatureMayAlwaysSet{feature_one}}));
  EXPECT_FALSE(frames.is_bottom());
  EXPECT_EQ(frames.callee_port(), AccessPath(Root(Root::Kind::Return)));
  EXPECT_EQ(
      frames,
      CalleePortFramesV2{test::make_taint_config(
          source_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one}})});

  // Add frame with the same kind
  frames.add(test::make_taint_config(
      source_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .origins = MethodSet{two},
          .inferred_features = FeatureMayAlwaysSet{feature_two},
          .user_features = FeatureSet{user_feature_one}}));
  EXPECT_EQ(
      frames,
      CalleePortFramesV2{test::make_taint_config(
          source_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .origins = MethodSet{one, two},
              .inferred_features =
                  FeatureMayAlwaysSet::make_may({feature_one, feature_two}),
              .user_features = FeatureSet{user_feature_one}})});

  // Add frame with a different kind
  frames.add(test::make_taint_config(
      source_kind_two,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .origins = MethodSet{two},
          .inferred_features = FeatureMayAlwaysSet{feature_two}}));
  EXPECT_EQ(
      frames,
      (CalleePortFramesV2{
          test::make_taint_config(
              source_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .origins = MethodSet{one, two},
                  .inferred_features =
                      FeatureMayAlwaysSet::make_may({feature_one, feature_two}),
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              source_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .origins = MethodSet{two},
                  .inferred_features = FeatureMayAlwaysSet{feature_two}})}));

  // Additional test for when callee_port == default value selected by
  // constructor in the implementation.
  frames = CalleePortFramesV2();
  frames.add(test::make_taint_config(source_kind_one, test::FrameProperties{}));
  EXPECT_EQ(frames.callee_port(), AccessPath(Root(Root::Kind::Leaf)));
}

TEST_F(CalleePortFramesV2Test, Leq) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));

  auto* test_kind_one = context.kinds->get("TestSinkOne");
  auto* test_kind_two = context.kinds->get("TestSinkTwo");

  // Comparison to bottom
  EXPECT_TRUE(CalleePortFramesV2::bottom().leq(CalleePortFramesV2::bottom()));
  EXPECT_TRUE(CalleePortFramesV2::bottom().leq(CalleePortFramesV2{
      test::make_taint_config(test_kind_one, test::FrameProperties{})}));
  EXPECT_FALSE((CalleePortFramesV2{test::make_taint_config(
                    test_kind_one, test::FrameProperties{})})
                   .leq(CalleePortFramesV2::bottom()));

  // Comparison to self
  EXPECT_TRUE(
      (CalleePortFramesV2{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
               .callee = one,
               .distance = 1,
               .origins = MethodSet{one}})})
          .leq(CalleePortFramesV2{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = one,
                  .distance = 1,
                  .origins = MethodSet{one}})}));

  // Different kinds
  EXPECT_TRUE(
      (CalleePortFramesV2{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
               .callee = one,
               .distance = 1,
               .origins = MethodSet{one}})})
          .leq(CalleePortFramesV2{
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = one,
                      .distance = 1,
                      .origins = MethodSet{one}}),
              test::make_taint_config(
                  test_kind_two,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = one,
                      .distance = 1,
                      .origins = MethodSet{one}}),
          }));
  EXPECT_FALSE(
      (CalleePortFramesV2{
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                   .callee = one,
                   .distance = 1,
                   .origins = MethodSet{one}}),
           test::make_taint_config(
               test_kind_two,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                   .callee = one,
                   .distance = 1,
                   .origins = MethodSet{one}})})
          .leq(CalleePortFramesV2{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = one,
                  .distance = 1,
                  .origins = MethodSet{one}})}));

  // Receiver sinks.
  EXPECT_TRUE(
      CalleePortFramesV2{
          test::make_taint_config(
              Kinds::receiver(),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .output_paths =
                      PathTreeDomain{
                          {Path{DexString::make_string("x")},
                           SingletonAbstractDomain()}}})}
          .leq(CalleePortFramesV2{test::make_taint_config(
              Kinds::receiver(),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .output_paths =
                      PathTreeDomain{{Path{}, SingletonAbstractDomain()}}})}));
  EXPECT_FALSE(
      CalleePortFramesV2{
          test::make_taint_config(
              Kinds::receiver(),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .output_paths =
                      PathTreeDomain{{Path{}, SingletonAbstractDomain()}}})}
          .leq(CalleePortFramesV2{test::make_taint_config(
              Kinds::receiver(),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .output_paths = PathTreeDomain{
                      {Path{DexString::make_string("x")},
                       SingletonAbstractDomain()}}})}));
}

TEST_F(CalleePortFramesV2Test, Equals) {
  auto context = test::make_empty_context();

  auto* test_kind_one = context.kinds->get("TestSinkOne");
  auto* test_kind_two = context.kinds->get("TestSinkTwo");
  const auto* x = DexString::make_string("x");
  const auto* y = DexString::make_string("y");
  const auto* z = DexString::make_string("z");

  // Comparison to bottom
  EXPECT_TRUE(
      CalleePortFramesV2::bottom().equals(CalleePortFramesV2::bottom()));
  EXPECT_FALSE(CalleePortFramesV2::bottom().equals(CalleePortFramesV2{
      test::make_taint_config(test_kind_one, test::FrameProperties{})}));
  EXPECT_FALSE((CalleePortFramesV2{test::make_taint_config(
                    test_kind_one, test::FrameProperties{})})
                   .equals(CalleePortFramesV2::bottom()));

  // Comparison to self
  EXPECT_TRUE((CalleePortFramesV2{test::make_taint_config(
                   test_kind_one, test::FrameProperties{})})
                  .equals(CalleePortFramesV2{test::make_taint_config(
                      test_kind_one, test::FrameProperties{})}));

  // Different kinds
  EXPECT_FALSE((CalleePortFramesV2{test::make_taint_config(
                    test_kind_one, test::FrameProperties{})})
                   .equals(CalleePortFramesV2{test::make_taint_config(
                       test_kind_two, test::FrameProperties{})}));

  // Receiver sink with different output path trees.
  EXPECT_FALSE(
      CalleePortFramesV2(
          {test::make_taint_config(
              Kinds::receiver(),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .output_paths = PathTreeDomain(
                      {{Path{x, y}, SingletonAbstractDomain()},
                       {Path{x, z}, SingletonAbstractDomain()}})})})
          .equals(CalleePortFramesV2{test::make_taint_config(
              Kinds::receiver(),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .output_paths =
                      PathTreeDomain{{Path{x}, SingletonAbstractDomain()}}})}));
}

TEST_F(CalleePortFramesV2Test, JoinWith) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));

  auto* test_kind_one = context.kinds->get("TestSinkOne");
  auto* test_kind_two = context.kinds->get("TestSinkTwo");

  // Join with bottom.
  EXPECT_EQ(
      CalleePortFramesV2::bottom().join(CalleePortFramesV2{
          test::make_taint_config(test_kind_one, test::FrameProperties{})}),
      CalleePortFramesV2{
          test::make_taint_config(test_kind_one, test::FrameProperties{})});

  EXPECT_EQ(
      (CalleePortFramesV2{
           test::make_taint_config(test_kind_one, test::FrameProperties{})})
          .join(CalleePortFramesV2::bottom()),
      CalleePortFramesV2{
          test::make_taint_config(test_kind_one, test::FrameProperties{})});

  // Additional test to verify that joining with bottom adopts the new port
  // and not the default "leaf" port.
  auto frames = (CalleePortFramesV2{test::make_taint_config(
                     test_kind_one,
                     test::FrameProperties{
                         .callee_port = AccessPath(Root(Root::Kind::Return))})})
                    .join(CalleePortFramesV2::bottom());
  EXPECT_EQ(
      frames,
      CalleePortFramesV2{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return))})});
  EXPECT_EQ(frames.callee_port(), AccessPath(Root(Root::Kind::Return)));

  frames = CalleePortFramesV2::bottom().join(
      CalleePortFramesV2{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return))})});
  EXPECT_EQ(
      frames,
      CalleePortFramesV2{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return))})});
  EXPECT_EQ(frames.callee_port(), AccessPath(Root(Root::Kind::Return)));

  // Join different kinds
  frames = CalleePortFramesV2{
      test::make_taint_config(test_kind_one, test::FrameProperties{})};
  frames.join_with(CalleePortFramesV2{
      test::make_taint_config(test_kind_two, test::FrameProperties{})});
  EXPECT_EQ(
      frames,
      (CalleePortFramesV2{
          test::make_taint_config(test_kind_one, test::FrameProperties{}),
          test::make_taint_config(test_kind_two, test::FrameProperties{})}));

  // Join same kind
  auto frame_one = test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
          .callee = one,
          .distance = 1,
          .origins = MethodSet{one}});
  auto frame_two = test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
          .callee = one,
          .distance = 2,
          .origins = MethodSet{one}});
  frames = CalleePortFramesV2{frame_one};
  frames.join_with(CalleePortFramesV2{frame_two});
  EXPECT_EQ(frames, (CalleePortFramesV2{frame_one}));
}

TEST_F(CalleePortFramesV2Test, ResultReceiverSinkJoinWith) {
  const auto* x = DexString::make_string("x");
  const auto* y = DexString::make_string("y");
  // Join output path trees for receiver/result sinks.
  auto frames = CalleePortFramesV2{test::make_taint_config(
      Kinds::receiver(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
          .output_paths =
              PathTreeDomain{{Path{x}, SingletonAbstractDomain()}}})};
  frames.join_with(CalleePortFramesV2{test::make_taint_config(
      Kinds::receiver(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
          .output_paths =
              PathTreeDomain{{Path{y}, SingletonAbstractDomain()}}})});
  EXPECT_EQ(
      frames.output_paths(),
      (PathTreeDomain{
          {Path{x}, SingletonAbstractDomain()},
          {Path{y}, SingletonAbstractDomain()}}));

  frames.join_with(CalleePortFramesV2{test::make_taint_config(
      Kinds::receiver(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
          .output_paths =
              PathTreeDomain{{Path{}, SingletonAbstractDomain()}}})});
  EXPECT_EQ(frames.output_paths(), PathTreeDomain{SingletonAbstractDomain()});
  EXPECT_EQ(
      frames,
      CalleePortFramesV2{test::make_taint_config(
          Kinds::receiver(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .output_paths =
                  PathTreeDomain{{Path{}, SingletonAbstractDomain()}}})});

  // Can't join receiver and result sinks together.
  EXPECT_THROW(
      frames.join_with(CalleePortFramesV2{test::make_taint_config(
          Kinds::receiver(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return))})}),
      std::exception);

  frames = CalleePortFramesV2{test::make_taint_config(
      Kinds::local_result(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths =
              PathTreeDomain{{Path{x, y}, SingletonAbstractDomain()}}})};
  frames.join_with(CalleePortFramesV2{test::make_taint_config(
      Kinds::local_result(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths =
              PathTreeDomain{{Path{x}, SingletonAbstractDomain()}}})});
  EXPECT_EQ(
      frames.output_paths(),
      (PathTreeDomain{{Path{x}, SingletonAbstractDomain()}}));
}

} // namespace marianatrench

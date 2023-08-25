/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/CalleePortFrames.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class CalleePortFramesTest : public test::Test {};

TEST_F(CalleePortFramesTest, Constructor) {
  auto context = test::make_empty_context();

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);

  // Verify local positions only need to be specified on one TaintBuilder in
  // order to apply to the whole object.
  auto frames = CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .local_positions = LocalPositionSet{test_position_one}}),
      test::make_taint_config(test_kind_two, test::FrameProperties{})};
  EXPECT_EQ(frames.local_positions(), LocalPositionSet{test_position_one});

  // Specifying the same position on both builders should have the same result.
  EXPECT_EQ(
      frames,
      (CalleePortFrames{
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
  frames = CalleePortFrames{
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

  // Specifying different local positions would result in them being joined and
  // applied to all frames.
}

TEST_F(CalleePortFramesTest, Add) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));
  auto* two = context.methods->create(
      redex::create_void_method(scope, "LOther;", "two"));

  auto* source_kind_one = context.kind_factory->get("TestSourceOne");
  auto* source_kind_two = context.kind_factory->get("TestSourceTwo");
  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");
  auto* user_feature_one = context.feature_factory->get("UserFeatureOne");

  CalleePortFrames frames;
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
      CalleePortFrames{test::make_taint_config(
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
      CalleePortFrames{test::make_taint_config(
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
      (CalleePortFrames{
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
  frames = CalleePortFrames();
  frames.add(test::make_taint_config(source_kind_one, test::FrameProperties{}));
  EXPECT_EQ(frames.callee_port(), AccessPath(Root(Root::Kind::Leaf)));
}

TEST_F(CalleePortFramesTest, Leq) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");

  // Comparison to bottom
  EXPECT_TRUE(CalleePortFrames::bottom().leq(CalleePortFrames::bottom()));
  EXPECT_TRUE(CalleePortFrames::bottom().leq(CalleePortFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{})}));
  EXPECT_FALSE((CalleePortFrames{test::make_taint_config(
                    test_kind_one, test::FrameProperties{})})
                   .leq(CalleePortFrames::bottom()));
  EXPECT_FALSE(
      (CalleePortFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Argument, 0))})})
          .leq(CalleePortFrames::bottom()));

  // Comparison to self
  EXPECT_TRUE(
      (CalleePortFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
               .callee = one,
               .distance = 1,
               .origins = MethodSet{one},
               .call_info = CallInfo::callsite()})})
          .leq(CalleePortFrames{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = one,
                  .distance = 1,
                  .origins = MethodSet{one},
                  .call_info = CallInfo::callsite()})}));

  // Different kinds
  EXPECT_TRUE(
      (CalleePortFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
               .callee = one,
               .distance = 1,
               .origins = MethodSet{one},
               .call_info = CallInfo::callsite()})})
          .leq(CalleePortFrames{
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = one,
                      .distance = 1,
                      .origins = MethodSet{one},
                      .call_info = CallInfo::callsite()}),
              test::make_taint_config(
                  test_kind_two,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = one,
                      .distance = 1,
                      .origins = MethodSet{one},
                      .call_info = CallInfo::callsite()}),
          }));
  EXPECT_FALSE(
      (CalleePortFrames{
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                   .callee = one,
                   .distance = 1,
                   .origins = MethodSet{one},
                   .call_info = CallInfo::callsite()}),
           test::make_taint_config(
               test_kind_two,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                   .callee = one,
                   .distance = 1,
                   .origins = MethodSet{one},
                   .call_info = CallInfo::callsite()})})
          .leq(CalleePortFrames{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = one,
                  .distance = 1,
                  .origins = MethodSet{one},
                  .call_info = CallInfo::callsite()})}));

  // Local kinds with output paths.
  EXPECT_TRUE(CalleePortFrames{
      test::make_taint_config(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .output_paths =
                  PathTreeDomain{
                      {Path{PathElement::field("x")}, CollapseDepth::zero()}},
              .call_info = CallInfo::propagation(),
          })}
                  .leq(CalleePortFrames{test::make_taint_config(
                      context.kind_factory->local_return(),
                      test::FrameProperties{
                          .callee_port = AccessPath(Root(Root::Kind::Return)),
                          .output_paths =
                              PathTreeDomain{{Path{}, CollapseDepth::zero()}},
                          .call_info = CallInfo::propagation(),
                      })}));
  EXPECT_FALSE(CalleePortFrames{
      test::make_taint_config(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              .call_info = CallInfo::propagation(),
          })}
                   .leq(CalleePortFrames{test::make_taint_config(
                       context.kind_factory->local_return(),
                       test::FrameProperties{
                           .callee_port = AccessPath(Root(Root::Kind::Return)),
                           .output_paths =
                               PathTreeDomain{
                                   {Path{PathElement::field("x")},
                                    CollapseDepth::zero()}},
                           .call_info = CallInfo::propagation(),
                       })}));
}

TEST_F(CalleePortFramesTest, Equals) {
  auto context = test::make_empty_context();

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");
  const auto z = PathElement::field("z");

  // Comparison to bottom
  EXPECT_TRUE(CalleePortFrames::bottom().equals(CalleePortFrames::bottom()));
  EXPECT_FALSE(CalleePortFrames::bottom().equals(CalleePortFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{})}));
  EXPECT_FALSE((CalleePortFrames{test::make_taint_config(
                    test_kind_one, test::FrameProperties{})})
                   .equals(CalleePortFrames::bottom()));

  // Comparison to self
  EXPECT_TRUE((CalleePortFrames{test::make_taint_config(
                   test_kind_one, test::FrameProperties{})})
                  .equals(CalleePortFrames{test::make_taint_config(
                      test_kind_one, test::FrameProperties{})}));

  // Different kinds
  EXPECT_FALSE((CalleePortFrames{test::make_taint_config(
                    test_kind_one, test::FrameProperties{})})
                   .equals(CalleePortFrames{test::make_taint_config(
                       test_kind_two, test::FrameProperties{})}));

  // Different output paths
  auto two_input_paths = CalleePortFrames(
      {test::make_taint_config(
           context.kind_factory->local_return(),
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Return)),
               .output_paths =
                   PathTreeDomain{{Path{x, y}, CollapseDepth::zero()}},
               .call_info = CallInfo::propagation(),
           }),
       test::make_taint_config(
           context.kind_factory->local_return(),
           test::FrameProperties{
               .callee_port = AccessPath(Root(Root::Kind::Return)),
               .output_paths =
                   PathTreeDomain{{Path{x, z}, CollapseDepth::zero()}},
               .call_info = CallInfo::propagation(),
           })});
  EXPECT_FALSE(two_input_paths.equals(CalleePortFrames{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths = PathTreeDomain{{Path{x}, CollapseDepth::zero()}},
          .call_info = CallInfo::propagation(),
      })}));
}

TEST_F(CalleePortFramesTest, JoinWith) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");

  // Join with bottom.
  EXPECT_EQ(
      CalleePortFrames::bottom().join(CalleePortFrames{
          test::make_taint_config(test_kind_one, test::FrameProperties{})}),
      CalleePortFrames{
          test::make_taint_config(test_kind_one, test::FrameProperties{})});

  EXPECT_EQ(
      (CalleePortFrames{
           test::make_taint_config(test_kind_one, test::FrameProperties{})})
          .join(CalleePortFrames::bottom()),
      CalleePortFrames{
          test::make_taint_config(test_kind_one, test::FrameProperties{})});

  // Additional test to verify that joining with bottom adopts the new port
  // and not the default "leaf" port.
  auto frames = (CalleePortFrames{test::make_taint_config(
                     test_kind_one,
                     test::FrameProperties{
                         .callee_port = AccessPath(Root(Root::Kind::Return))})})
                    .join(CalleePortFrames::bottom());
  EXPECT_EQ(
      frames,
      CalleePortFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return))})});
  EXPECT_EQ(frames.callee_port(), AccessPath(Root(Root::Kind::Return)));

  frames =
      CalleePortFrames::bottom().join(CalleePortFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return))})});
  EXPECT_EQ(
      frames,
      CalleePortFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return))})});
  EXPECT_EQ(frames.callee_port(), AccessPath(Root(Root::Kind::Return)));

  // Join different kinds
  frames = CalleePortFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{})};
  frames.join_with(CalleePortFrames{
      test::make_taint_config(test_kind_two, test::FrameProperties{})});
  EXPECT_EQ(
      frames,
      (CalleePortFrames{
          test::make_taint_config(test_kind_one, test::FrameProperties{}),
          test::make_taint_config(test_kind_two, test::FrameProperties{})}));

  // Join same kind
  auto frame_one = test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
          .callee = one,
          .distance = 1,
          .origins = MethodSet{one},
          .call_info = CallInfo::callsite(),
      });
  auto frame_two = test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
          .callee = one,
          .distance = 2,
          .origins = MethodSet{one},
          .call_info = CallInfo::callsite(),
      });
  frames = CalleePortFrames{frame_one};
  frames.join_with(CalleePortFrames{frame_two});
  EXPECT_EQ(frames, (CalleePortFrames{frame_one}));
}

TEST_F(CalleePortFramesTest, Difference) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));
  auto* three = context.methods->create(
      redex::create_void_method(scope, "LThree;", "three"));

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");
  auto* user_feature_one = context.feature_factory->get("UserFeatureOne");
  auto* user_feature_two = context.feature_factory->get("UserFeatureTwo");

  CalleePortFrames frames, initial_frames;

  // Tests with empty left hand side.
  frames.difference_with(CalleePortFrames{});
  EXPECT_TRUE(frames.is_bottom());

  frames.difference_with(CalleePortFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
  });
  EXPECT_TRUE(frames.is_bottom());

  initial_frames = CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one},
              .call_info = CallInfo::callsite()}),
  };

  frames = initial_frames;
  frames.difference_with(CalleePortFrames::bottom());
  EXPECT_EQ(frames, initial_frames);

  frames = initial_frames;
  frames.difference_with(CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one},
              .call_info = CallInfo::callsite()}),
  });
  EXPECT_TRUE(frames.is_bottom());

  // Left hand side is bigger than right hand side.
  frames = initial_frames;
  frames.difference_with(CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{one},
              .call_info = CallInfo::callsite()}),
  });
  EXPECT_EQ(frames, initial_frames);

  // Left hand side and right hand side have different inferred features.
  frames = initial_frames;
  frames.difference_with(CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_one},
              .call_info = CallInfo::callsite()}),
  });
  EXPECT_EQ(frames, initial_frames);

  // Left hand side and right hand side have different user features.
  frames = initial_frames;
  frames.difference_with(CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_two},
              .call_info = CallInfo::callsite()}),
  });
  EXPECT_EQ(frames, initial_frames);

  // Left hand side is smaller than right hand side (with one kind).
  frames = CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one},
              .call_info = CallInfo::callsite()}),
  };
  frames.difference_with(CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one},
              .call_info = CallInfo::callsite()}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{two},
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_two},
              .call_info = CallInfo::callsite()}),
  });
  EXPECT_TRUE(frames.is_bottom());

  // Left hand side has more kinds than right hand side.
  frames = CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{one},
              .call_info = CallInfo::callsite()}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{one},
              .call_info = CallInfo::callsite()}),
  };
  frames.difference_with(CalleePortFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
          .callee = one,
          .distance = 1,
          .origins = MethodSet{one},
          .call_info = CallInfo::callsite(),
      })});
  EXPECT_EQ(
      frames,
      (CalleePortFrames{
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = one,
                  .distance = 1,
                  .origins = MethodSet{one},
                  .call_info = CallInfo::callsite(),
              }),
      }));

  // Left hand side is smaller for one kind, and larger for another.
  frames = CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{one},
              .call_info = CallInfo::callsite(),
          }),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{two, three},
              .call_info = CallInfo::callsite(),
          }),
  };
  frames.difference_with(CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{one, two},
              .call_info = CallInfo::callsite(),
          }),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{two},
              .call_info = CallInfo::callsite(),
          })});
  EXPECT_EQ(
      frames,
      (CalleePortFrames{test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{two, three},
              .call_info = CallInfo::callsite(),
          })}));
}

TEST_F(CalleePortFramesTest, DifferenceLocalPositions) {
  auto context = test::make_empty_context();

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);

  // Empty left hand side.
  auto frames = CalleePortFrames::bottom();
  frames.difference_with(CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .local_positions = LocalPositionSet{test_position_one}}),
  });
  EXPECT_TRUE(frames.is_bottom());

  // lhs.local_positions <= rhs.local_positions
  // lhs.frames <= rhs.frames
  frames = CalleePortFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{})};
  frames.difference_with(CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .local_positions = LocalPositionSet{test_position_one}}),
      test::make_taint_config(test_kind_two, test::FrameProperties{})});
  EXPECT_TRUE(frames.is_bottom());

  // lhs.local_positions <= rhs.local_positions
  // lhs.frames > rhs.frames
  frames = CalleePortFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
      test::make_taint_config(test_kind_two, test::FrameProperties{})};
  frames.difference_with(CalleePortFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .local_positions = LocalPositionSet{test_position_one}})});
  EXPECT_EQ(
      frames,
      CalleePortFrames{
          test::make_taint_config(test_kind_two, test::FrameProperties{})});

  // lhs.local_positions > rhs.local_positions
  // lhs.frames > rhs.frames
  frames = CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .local_positions = LocalPositionSet{test_position_one}}),
      test::make_taint_config(test_kind_two, test::FrameProperties{})};
  frames.difference_with(CalleePortFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{})});
  EXPECT_EQ(
      frames,
      (CalleePortFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .local_positions = LocalPositionSet{test_position_one}}),
          test::make_taint_config(test_kind_two, test::FrameProperties{})}));

  // lhs.local_positions > rhs.local_positions
  // lhs.frames <= rhs.frames
  frames = CalleePortFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .local_positions = LocalPositionSet{test_position_one}})};
  frames.difference_with(CalleePortFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
      test::make_taint_config(test_kind_two, test::FrameProperties{})});
  EXPECT_EQ(
      frames,
      CalleePortFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .local_positions = LocalPositionSet{test_position_one}})});
}

TEST_F(CalleePortFramesTest, DifferenceOutputPaths) {
  auto context = test::make_empty_context();

  const auto x = PathElement::field("x");
  const auto* feature = context.feature_factory->get("featureone");

  // lhs.output_paths <= rhs.output_paths
  // lhs.frames <= rhs.frames
  auto lhs_frames = CalleePortFrames{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths = PathTreeDomain{{Path{x}, CollapseDepth::zero()}},
          .call_info = CallInfo::propagation(),
      })};
  lhs_frames.difference_with(CalleePortFrames{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          .call_info = CallInfo::propagation(),
      })});
  EXPECT_TRUE(lhs_frames.is_bottom());

  // lhs.output_paths <= rhs.output_paths
  // lhs.frames > rhs.frames
  lhs_frames = CalleePortFrames{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .inferred_features = FeatureMayAlwaysSet{feature},
          .output_paths = PathTreeDomain{{Path{x}, CollapseDepth::zero()}},
          .call_info = CallInfo::propagation(),
      })};
  lhs_frames.difference_with(CalleePortFrames{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          .call_info = CallInfo::propagation(),
      })});
  EXPECT_EQ(
      lhs_frames,
      CalleePortFrames{test::make_taint_config(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .inferred_features = FeatureMayAlwaysSet{feature},
              .output_paths = PathTreeDomain{{Path{x}, CollapseDepth::zero()}},
              .call_info = CallInfo::propagation(),
          })});

  // lhs.output_paths > rhs.output_paths
  // lhs.frames <= rhs.frames
  lhs_frames = CalleePortFrames{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          .call_info = CallInfo::propagation(),
      })};
  lhs_frames.difference_with(CalleePortFrames{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths = PathTreeDomain{{Path{x}, CollapseDepth::zero()}},
          .call_info = CallInfo::propagation(),
      })});
  EXPECT_EQ(
      lhs_frames,
      CalleePortFrames{test::make_taint_config(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              .call_info = CallInfo::propagation(),
          })});

  // lhs.output_paths > rhs.output_paths
  // lhs.frames > rhs.frames
  lhs_frames = CalleePortFrames{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .inferred_features = FeatureMayAlwaysSet{feature},
          .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          .call_info = CallInfo::propagation(),
      })};
  lhs_frames.difference_with(CalleePortFrames{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths = PathTreeDomain{{Path{x}, CollapseDepth::zero()}},
          .call_info = CallInfo::propagation(),
      })});
  EXPECT_EQ(
      lhs_frames,
      CalleePortFrames{test::make_taint_config(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .inferred_features = FeatureMayAlwaysSet{feature},
              .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              .call_info = CallInfo::propagation(),
          })});
}

TEST_F(CalleePortFramesTest, Iterator) {
  auto context = test::make_empty_context();

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");

  auto callee_port_frames = CalleePortFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
      test::make_taint_config(test_kind_two, test::FrameProperties{})};

  std::vector<Frame> frames;
  for (const auto& frame : callee_port_frames) {
    frames.push_back(frame);
  }

  EXPECT_EQ(frames.size(), 2);
  EXPECT_NE(
      std::find(
          frames.begin(),
          frames.end(),
          test::make_taint_frame(test_kind_one, test::FrameProperties{})),
      frames.end());
  EXPECT_NE(
      std::find(
          frames.begin(),
          frames.end(),
          test::make_taint_frame(test_kind_two, test::FrameProperties{})),
      frames.end());
}

TEST_F(CalleePortFramesTest, Map) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* feature_one = context.feature_factory->get("FeatureOne");

  auto frames = CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{one},
              .call_info = CallInfo::callsite()}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 2,
              .origins = MethodSet{one},
              .call_info = CallInfo::callsite()}),
  };
  frames.map([feature_one](Frame frame) {
    frame.add_inferred_features(FeatureMayAlwaysSet{feature_one});
    return frame;
  });
  EXPECT_EQ(
      frames,
      (CalleePortFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = one,
                  .distance = 1,
                  .origins = MethodSet{one},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .call_info = CallInfo::callsite(),
              }),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = one,
                  .distance = 2,
                  .origins = MethodSet{one},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .call_info = CallInfo::callsite(),
              }),
      }));
}

TEST_F(CalleePortFramesTest, FeaturesAndPositions) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);
  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");

  // add_locally_inferred_features should be an *add* operation on the features,
  // not a join.
  auto frames = CalleePortFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .locally_inferred_features = FeatureMayAlwaysSet(
              /* may */ FeatureSet{feature_one},
              /* always */ FeatureSet{})})};
  frames.add_locally_inferred_features(FeatureMayAlwaysSet{feature_two});
  EXPECT_EQ(
      frames.locally_inferred_features(),
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{feature_one},
          /* always */ FeatureSet{feature_two}));

  frames = CalleePortFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .local_positions = LocalPositionSet{test_position_one}}),
  };
  EXPECT_EQ(frames.local_positions(), (LocalPositionSet{test_position_one}));

  frames.add_local_position(test_position_two);
  EXPECT_EQ(
      frames.local_positions(),
      (LocalPositionSet{test_position_one, test_position_two}));

  frames.set_local_positions(LocalPositionSet{test_position_two});
  EXPECT_EQ(frames.local_positions(), (LocalPositionSet{test_position_two}));
}

TEST_F(CalleePortFramesTest, Propagate) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* call_position = context.positions->get("Test.java", 1);

  // Test propagating non-crtex frames. Crtex-ness determined by callee port.
  auto non_crtex_frames = CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .distance = 1,
              .origins = MethodSet{one},
              .call_info = CallInfo::callsite()}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .origins = MethodSet{one}, .call_info = CallInfo::origin()}),
  };
  EXPECT_EQ(
      non_crtex_frames.propagate(
          /* callee */ two,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          call_position,
          /* maximum_source_sink_distance */ 100,
          context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {},
          CallClassIntervalContext(),
          ClassIntervals::Interval::top()),
      (CalleePortFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = call_position,
                  .distance = 2,
                  .origins = MethodSet{one},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .call_info = CallInfo::callsite()}),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = call_position,
                  .distance = 1,
                  .origins = MethodSet{one},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .call_info = CallInfo::callsite()}),
      }));

  // Test propagating crtex frames (callee port == anchor).
  auto crtex_frames = CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Anchor)),
              .origins = MethodSet{one},
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"%programmatic_leaf_name%"})},
              .call_info = CallInfo::origin()}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Anchor)),
              .origins = MethodSet{one},
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"constant value"})},
              .call_info = CallInfo::origin()}),
  };

  auto expected_instantiated_name =
      CanonicalName(CanonicalName::InstantiatedValue{two->signature()});
  auto propagated_crtex_frames = crtex_frames.propagate(
      /* callee */ two,
      /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
      call_position,
      /* maximum_source_sink_distance */ 100,
      context,
      /* source_register_types */ {},
      /* source_constant_arguments */ {},
      CallClassIntervalContext(),
      ClassIntervals::Interval::top());
  EXPECT_EQ(
      propagated_crtex_frames,
      (CalleePortFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(
                      Root(Root::Kind::Anchor),
                      Path{PathElement::field("Argument(-1)")}),
                  .callee = two,
                  .call_position = call_position,
                  .origins = MethodSet{one},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .canonical_names =
                      CanonicalNameSetAbstractDomain{
                          expected_instantiated_name},
                  .call_info = CallInfo::callsite()}),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(
                      Root(Root::Kind::Anchor),
                      Path{PathElement::field("Argument(-1)")}),
                  .callee = two,
                  .call_position = call_position,
                  .origins = MethodSet{one},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .canonical_names =
                      CanonicalNameSetAbstractDomain(CanonicalName(
                          CanonicalName::InstantiatedValue{"constant value"})),
                  .call_info = CallInfo::callsite()}),
      }));

  // Test propagating crtex-like frames (callee port == anchor.<path>),
  // specifically, propagate the propagated frames above again. These frames
  // originate from crtex leaves, but are not themselves the leaves.
  EXPECT_EQ(
      propagated_crtex_frames.propagate(
          /* callee */ two,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          call_position,
          /* maximum_source_sink_distance */ 100,
          context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {},
          CallClassIntervalContext(),
          ClassIntervals::Interval::top()),
      (CalleePortFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = call_position,
                  .distance = 1,
                  .origins = MethodSet{one},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .call_info = CallInfo::callsite()}),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = call_position,
                  .distance = 1,
                  .origins = MethodSet{one},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .call_info = CallInfo::callsite()}),
      }));
}

TEST_F(CalleePortFramesTest, PropagateDropFrames) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* call_position = context.positions->get("Test.java", 1);
  auto* user_feature_one = context.feature_factory->get("UserFeatureOne");
  auto* user_feature_two = context.feature_factory->get("UserFeatureTwo");

  // Propagating this frame will give it a distance of 2. It is expected to be
  // dropped as it exceeds the maximum distance allowed.
  auto frames = CalleePortFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee = one, .distance = 1, .call_info = CallInfo::callsite()})};
  EXPECT_EQ(
      frames.propagate(
          /* callee */ two,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          call_position,
          /* maximum_source_sink_distance */ 1,
          context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {},
          CallClassIntervalContext(),
          ClassIntervals::Interval::top()),
      CalleePortFrames::bottom());

  // One of the two frames will be ignored during propagation because its
  // distance exceeds the maximum distance allowed.
  frames = CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .distance = 2,
              .user_features = FeatureSet{user_feature_one},
              .call_info = CallInfo::callsite()}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee = one,
              .distance = 1,
              .user_features = FeatureSet{user_feature_two},
              .call_info = CallInfo::callsite()}),
  };
  EXPECT_EQ(
      frames.propagate(
          /* callee */ two,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          call_position,
          /* maximum_source_sink_distance */ 2,
          context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {},
          CallClassIntervalContext(),
          ClassIntervals::Interval::top()),
      (CalleePortFrames{
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = call_position,
                  .distance = 2,
                  .inferred_features = FeatureMayAlwaysSet{user_feature_two},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .call_info = CallInfo::callsite()}),
      }));
}

TEST_F(CalleePortFramesTest, TransformKindWithFeatures) {
  auto context = test::make_empty_context();

  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");
  auto* user_feature_one = context.feature_factory->get("UserFeatureOne");

  auto* test_kind_one = context.kind_factory->get("TestKindOne");
  auto* test_kind_two = context.kind_factory->get("TestKindTwo");
  auto* transformed_test_kind_one =
      context.kind_factory->get("TransformedTestKindOne");
  auto* transformed_test_kind_two =
      context.kind_factory->get("TransformedTestKindTwo");

  auto initial_frames = CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.user_features = FeatureSet{user_feature_one}}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one}}),
  };

  // Drop all kinds.
  auto frames = initial_frames;
  frames.transform_kind_with_features(
      [](const auto* /* unused kind */) { return std::vector<const Kind*>(); },
      [](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet::bottom();
      });
  EXPECT_EQ(frames, CalleePortFrames::bottom());

  // Perform an actual transformation.
  frames = initial_frames;
  frames.transform_kind_with_features(
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
      frames,
      (CalleePortFrames{
          test::make_taint_config(
              transformed_test_kind_one,
              test::FrameProperties{
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
      }));

  // Another transformation, this time including a change to the features
  frames = initial_frames;
  frames.transform_kind_with_features(
      [&](const auto* kind) {
        if (kind == test_kind_one) {
          return std::vector<const Kind*>{transformed_test_kind_one};
        }
        return std::vector<const Kind*>{kind};
      },
      [feature_two](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet{feature_two};
      });
  EXPECT_EQ(
      frames,
      (CalleePortFrames{
          test::make_taint_config(
              transformed_test_kind_one,
              test::FrameProperties{
                  .inferred_features = FeatureMayAlwaysSet{feature_two},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
      }));

  // Similar transformation, but with may-features. The desired behavior for
  // inferred features is an "add", not a "join"
  frames = initial_frames;
  frames.transform_kind_with_features(
      [&](const auto* kind) {
        if (kind == test_kind_two) {
          return std::vector<const Kind*>{transformed_test_kind_two};
        }
        return std::vector<const Kind*>{kind};
      },
      [feature_two](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet::make_may({feature_two});
      });
  EXPECT_EQ(
      frames,
      (CalleePortFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              transformed_test_kind_two,
              test::FrameProperties{
                  .inferred_features = FeatureMayAlwaysSet(
                      /* may */ FeatureSet{feature_two},
                      /* always */ FeatureSet{feature_one}),
                  .user_features = FeatureSet{user_feature_one}}),
      }));

  // Tests one -> many transformations (with features).
  frames = initial_frames;
  frames.transform_kind_with_features(
      [&](const auto* kind) {
        if (kind == test_kind_one) {
          return std::vector<const Kind*>{
              test_kind_one,
              transformed_test_kind_one,
              transformed_test_kind_two};
        }
        return std::vector<const Kind*>{};
      },
      [feature_two](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet{feature_two};
      });
  EXPECT_EQ(
      frames,
      (CalleePortFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .inferred_features = FeatureMayAlwaysSet{feature_two},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              transformed_test_kind_one,
              test::FrameProperties{
                  .inferred_features = FeatureMayAlwaysSet{feature_two},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              transformed_test_kind_two,
              test::FrameProperties{
                  .inferred_features = FeatureMayAlwaysSet{feature_two},
                  .user_features = FeatureSet{user_feature_one}}),
      }));

  // Tests transformations with features added to specific kinds.
  frames = initial_frames;
  frames.transform_kind_with_features(
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
      frames,
      (CalleePortFrames{
          test::make_taint_config(
              transformed_test_kind_one,
              test::FrameProperties{
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              transformed_test_kind_two,
              test::FrameProperties{
                  .user_features = FeatureSet{user_feature_one}}),
      }));

  // Transformation where multiple old kinds map to the same new kind
  frames = CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_one}}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
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
      (CalleePortFrames{
          test::make_taint_config(
              transformed_test_kind_one,
              test::FrameProperties{
                  .inferred_features = FeatureMayAlwaysSet(
                      /* may */ FeatureSet{feature_one, feature_two},
                      /* always */ FeatureSet{}),
                  .user_features = FeatureSet{user_feature_one}}),
      }));
}

TEST_F(CalleePortFramesTest, AppendOutputPaths) {
  auto context = test::make_empty_context();

  const auto path_element1 = PathElement::field("field1");
  const auto path_element2 = PathElement::field("field2");

  auto frames = CalleePortFrames{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths =
              PathTreeDomain{{Path{path_element1}, CollapseDepth(4)}},
          .call_info = CallInfo::propagation(),
      })};

  frames.append_to_propagation_output_paths(path_element2);
  EXPECT_EQ(
      frames,
      (CalleePortFrames{test::make_taint_config(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .output_paths =
                  PathTreeDomain{
                      {Path{path_element1, path_element2}, CollapseDepth(3)}},
              .call_info = CallInfo::propagation(),
          })}));
}

TEST_F(CalleePortFramesTest, FilterInvalidFrames) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method1 =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* test_kind_one = context.kind_factory->get("TestSourceOne");
  auto* test_kind_two = context.kind_factory->get("TestSourceTwo");

  // Filter by callee. In practice, this scenario where the frames each contain
  // a different callee will not happen. These frames will be never show up in
  // the same `CalleePortFrames` object.
  //
  // TODO(T91357916): Move callee, call_position and callee_port out of `Frame`
  // and re-visit these tests. Signature of `filter_invalid_frames` will likely
  // change.
  auto frames = CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument))}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .callee = method1,
              .distance = 1,
              .call_info = CallInfo::callsite()})};
  frames.filter_invalid_frames(
      /* is_valid */
      [&](const Method* MT_NULLABLE callee,
          const AccessPath& /* callee_port */,
          const Kind* /* kind */) { return callee == nullptr; });
  EXPECT_EQ(
      frames,
      (CalleePortFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument))})}));

  // Filter by callee port (drops nothing)
  frames = CalleePortFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument)),
          .callee = method1,
          .distance = 1,
          .call_info = CallInfo::callsite()})};
  frames.filter_invalid_frames(
      /* is_valid */
      [&](const Method* MT_NULLABLE /* callee */,
          const AccessPath& callee_port,
          const Kind* /* kind */) {
        return callee_port == AccessPath(Root(Root::Kind::Argument));
      });
  EXPECT_EQ(
      frames,
      (CalleePortFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .callee = method1,
              .distance = 1,
              .call_info = CallInfo::callsite()})}));

  // Filter by callee port (drops everything)
  frames = CalleePortFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Argument)),
          .callee = method1,
          .distance = 1,
          .call_info = CallInfo::callsite()})};
  frames.filter_invalid_frames(
      /* is_valid */
      [&](const Method* MT_NULLABLE /* callee */,
          const AccessPath& callee_port,
          const Kind* /* kind */) {
        return callee_port != AccessPath(Root(Root::Kind::Argument));
      });
  EXPECT_EQ(frames, CalleePortFrames::bottom());

  // Filter by kind
  frames = CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument))}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .callee = method1,
              .call_info = CallInfo::callsite(),
          })};
  frames.filter_invalid_frames(
      /* is_valid */
      [&](const Method* MT_NULLABLE /* callee */,
          const AccessPath& /* callee_port */,
          const Kind* kind) { return kind != test_kind_two; });
  EXPECT_EQ(
      frames,
      (CalleePortFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument))})}));
}

TEST_F(CalleePortFramesTest, Show) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* test_kind_one = context.kind_factory->get("TestSink1");
  auto frame_one = test::make_taint_config(
      test_kind_one, test::FrameProperties{.origins = MethodSet{one}});
  auto frames = CalleePortFrames{frame_one};

  EXPECT_EQ(
      show(frames),
      "CalleePortFrames(callee_port=AccessPath(Leaf), frames=[KindFrames("
      "frames=[FramesByInterval(interval={T, preserves_type_context=0}, "
      "frame=Frame(kind=`TestSink1`, callee_port=AccessPath(Leaf), "
      "class_interval_context={T, preserves_type_context=0}, call_info="
      "Declaration, origins={`LOne;.one:()V`})),]),])");

  frames = CalleePortFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .origins = MethodSet{one},
          .local_positions =
              LocalPositionSet{context.positions->get(std::nullopt, 1)}})};
  EXPECT_EQ(
      show(frames),
      "CalleePortFrames(callee_port=AccessPath(Leaf), "
      "local_positions={Position(line=1)}, frames=[KindFrames(frames=["
      "FramesByInterval(interval={T, preserves_type_context=0}, frame=Frame("
      "kind=`TestSink1`, callee_port=AccessPath(Leaf), class_interval_context="
      "{T, preserves_type_context=0}, call_info=Declaration, "
      "origins={`LOne;.one:()V`})),]),])");

  frames = CalleePortFrames{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
          .call_info = CallInfo::propagation(),
      })};
  EXPECT_EQ(
      show(frames),
      "CalleePortFrames(callee_port=AccessPath(Return), frames=[KindFrames("
      "frames=[FramesByInterval(interval={T, preserves_type_context=0}, "
      "frame=Frame(kind=`LocalReturn`, callee_port=AccessPath(Return), "
      "class_interval_context={T, preserves_type_context=0}, call_info="
      "Propagation, output_paths={0})),]),])");

  frames = CalleePortFrames{test::make_taint_config(
      context.kind_factory->local_return(),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .output_paths =
              PathTreeDomain{
                  {Path{PathElement::field("x")}, CollapseDepth::zero()}},
          .call_info = CallInfo::propagation(),
      })};
  EXPECT_EQ(
      show(frames),
      "CalleePortFrames(callee_port=AccessPath(Return), frames=[KindFrames("
      "frames=[FramesByInterval(interval={T, preserves_type_context=0}, "
      "frame=Frame(kind=`LocalReturn`, callee_port=AccessPath(Return), "
      "class_interval_context={T, preserves_type_context=0}, call_info="
      "Propagation, output_paths={\n    `.x` -> {0}\n})),]),])");

  EXPECT_EQ(
      show(CalleePortFrames::bottom()),
      "CalleePortFrames(callee_port=AccessPath(Leaf), frames=[])");
}

TEST_F(CalleePortFramesTest, ContainsKind) {
  auto context = test::make_empty_context();

  auto frames = CalleePortFrames{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSourceOne"),
          test::FrameProperties{}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSourceTwo"),
          test::FrameProperties{})};

  EXPECT_TRUE(frames.contains_kind(context.kind_factory->get("TestSourceOne")));
  EXPECT_TRUE(frames.contains_kind(context.kind_factory->get("TestSourceTwo")));
  EXPECT_FALSE(frames.contains_kind(context.kind_factory->get("TestSink")));
}

TEST_F(CalleePortFramesTest, PartitionByKind) {
  auto context = test::make_empty_context();

  auto* test_kind_one = context.kind_factory->get("TestSource1");
  auto* test_kind_two = context.kind_factory->get("TestSource2");

  auto frames = CalleePortFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return))}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return))}),
  };

  auto frames_by_kind = frames.partition_by_kind<const Kind*>(
      [](const Kind* kind) { return kind; });
  EXPECT_TRUE(frames_by_kind.size() == 2);
  EXPECT_EQ(
      frames_by_kind[test_kind_one],
      CalleePortFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return))})});
  EXPECT_EQ(
      frames_by_kind[test_kind_one].callee_port(),
      AccessPath(Root(Root::Kind::Return)));
  EXPECT_EQ(
      frames_by_kind[test_kind_two],
      CalleePortFrames{test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return))})});
  EXPECT_EQ(
      frames_by_kind[test_kind_two].callee_port(),
      AccessPath(Root(Root::Kind::Return)));
}

} // namespace marianatrench

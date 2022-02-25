/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>
#include <gmock/gmock.h>

#include <mariana-trench/CalleeFrames.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class CalleeFramesTest : public test::Test {};

TEST_F(CalleeFramesTest, Add) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));

  auto* source_kind_one = context.kinds->get("TestSourceOne");
  auto* source_kind_two = context.kinds->get("TestSourceTwo");

  auto* position_one = context.positions->get(std::nullopt, 1);

  CalleeFrames frames;
  EXPECT_TRUE(frames.is_bottom());
  EXPECT_TRUE(frames.empty());
  EXPECT_EQ(frames.callee(), nullptr);

  frames.add(test::make_frame(source_kind_one, test::FrameProperties{}));
  EXPECT_FALSE(frames.is_bottom());
  EXPECT_EQ(frames.callee(), nullptr);
  EXPECT_EQ(
      frames,
      CalleeFrames{test::make_frame(source_kind_one, test::FrameProperties{})});

  // Add frame with the same position (nullptr), different kind
  frames.add(test::make_frame(
      source_kind_two, test::FrameProperties{.origins = MethodSet{one}}));
  EXPECT_EQ(
      frames,
      (CalleeFrames{
          test::make_frame(source_kind_one, test::FrameProperties{}),
          test::make_frame(
              source_kind_two,
              test::FrameProperties{.origins = MethodSet{one}}),
      }));

  // Add frame with a different position
  frames.add(test::make_frame(
      source_kind_two, test::FrameProperties{.call_position = position_one}));
  EXPECT_EQ(
      frames,
      (CalleeFrames{
          test::make_frame(source_kind_one, test::FrameProperties{}),
          test::make_frame(
              source_kind_two,
              test::FrameProperties{.origins = MethodSet{one}}),
          test::make_frame(
              source_kind_two,
              test::FrameProperties{.call_position = position_one}),
      }));
}

TEST_F(CalleeFramesTest, Leq) {
  auto context = test::make_empty_context();

  auto* test_kind_one = context.kinds->get("TestSinkOne");
  auto* test_kind_two = context.kinds->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);

  // Comparison to bottom
  EXPECT_TRUE(CalleeFrames::bottom().leq(CalleeFrames::bottom()));
  EXPECT_TRUE(CalleeFrames::bottom().leq(
      CalleeFrames{test::make_frame(test_kind_one, test::FrameProperties{})}));
  EXPECT_FALSE((CalleeFrames{test::make_frame(
                    test_kind_one,
                    test::FrameProperties{.call_position = test_position_one})})
                   .leq(CalleeFrames::bottom()));

  // Comparison to self
  EXPECT_TRUE(
      (CalleeFrames{test::make_frame(test_kind_one, test::FrameProperties{})})
          .leq(CalleeFrames{
              test::make_frame(test_kind_one, test::FrameProperties{})}));

  // Same position, different kinds
  EXPECT_TRUE(
      (CalleeFrames{test::make_frame(
           test_kind_one,
           test::FrameProperties{.call_position = test_position_one})})
          .leq(CalleeFrames{
              test::make_frame(
                  test_kind_one,
                  test::FrameProperties{.call_position = test_position_one}),
              test::make_frame(
                  test_kind_two,
                  test::FrameProperties{.call_position = test_position_one}),
          }));
  EXPECT_FALSE(
      (CalleeFrames{
           test::make_frame(
               test_kind_one,
               test::FrameProperties{.call_position = test_position_one}),
           test::make_frame(
               test_kind_two,
               test::FrameProperties{.call_position = test_position_one}),
       })
          .leq(CalleeFrames{test::make_frame(
              test_kind_one,
              test::FrameProperties{.call_position = test_position_one})}));

  // Different positions
  EXPECT_TRUE(
      (CalleeFrames{test::make_frame(
           test_kind_one,
           test::FrameProperties{.call_position = test_position_one})})
          .leq(CalleeFrames{
              test::make_frame(
                  test_kind_one,
                  test::FrameProperties{.call_position = test_position_one}),
              test::make_frame(
                  test_kind_one,
                  test::FrameProperties{.call_position = test_position_two}),
          }));
  EXPECT_FALSE(
      (CalleeFrames{
           test::make_frame(
               test_kind_one,
               test::FrameProperties{.call_position = test_position_one}),
           test::make_frame(
               test_kind_one,
               test::FrameProperties{.call_position = test_position_two})})
          .leq(CalleeFrames{test::make_frame(
              test_kind_one,
              test::FrameProperties{.call_position = test_position_one})}));
}

TEST_F(CalleeFramesTest, Equals) {
  auto context = test::make_empty_context();

  auto* test_kind_one = context.kinds->get("TestSinkOne");
  auto* test_kind_two = context.kinds->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);

  // Comparison to bottom
  EXPECT_TRUE(CalleeFrames::bottom().equals(CalleeFrames::bottom()));
  EXPECT_FALSE(CalleeFrames::bottom().equals(CalleeFrames{test::make_frame(
      test_kind_one,
      test::FrameProperties{.call_position = test_position_one})}));
  EXPECT_FALSE((CalleeFrames{test::make_frame(
                    test_kind_one,
                    test::FrameProperties{.call_position = test_position_one})})
                   .equals(CalleeFrames::bottom()));

  // Comparison to self
  EXPECT_TRUE(
      (CalleeFrames{test::make_frame(test_kind_one, test::FrameProperties{})})
          .equals(CalleeFrames{
              test::make_frame(test_kind_one, test::FrameProperties{})}));

  // Different positions
  EXPECT_FALSE(
      (CalleeFrames{test::make_frame(
           test_kind_one,
           test::FrameProperties{.call_position = test_position_one})})
          .equals(CalleeFrames{test::make_frame(
              test_kind_one,
              test::FrameProperties{.call_position = test_position_two})}));

  // Different kinds
  EXPECT_FALSE(
      (CalleeFrames{test::make_frame(test_kind_one, test::FrameProperties{})})
          .equals(CalleeFrames{
              test::make_frame(test_kind_two, test::FrameProperties{})}));
}

TEST_F(CalleeFramesTest, JoinWith) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));

  auto* test_kind_one = context.kinds->get("TestSinkOne");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);
  auto* feature_one = context.features->get("FeatureOne");
  auto* feature_two = context.features->get("FeatureTwo");

  // Join with bottom
  EXPECT_EQ(
      CalleeFrames::bottom().join(CalleeFrames{
          test::make_frame(test_kind_one, test::FrameProperties{})}),
      CalleeFrames{test::make_frame(test_kind_one, test::FrameProperties{})});

  EXPECT_EQ(
      (CalleeFrames{test::make_frame(test_kind_one, test::FrameProperties{})})
          .join(CalleeFrames::bottom()),
      CalleeFrames{test::make_frame(test_kind_one, test::FrameProperties{})});

  auto frames = (CalleeFrames{test::make_frame(
                     test_kind_one, test::FrameProperties{.callee = one})})
                    .join(CalleeFrames::bottom());
  EXPECT_EQ(
      frames,
      CalleeFrames{test::make_frame(
          test_kind_one, test::FrameProperties{.callee = one})});
  EXPECT_EQ(frames.callee(), one);

  frames = CalleeFrames::bottom().join(CalleeFrames{
      test::make_frame(test_kind_one, test::FrameProperties{.callee = one})});
  EXPECT_EQ(
      frames,
      CalleeFrames{test::make_frame(
          test_kind_one, test::FrameProperties{.callee = one})});
  EXPECT_EQ(frames.callee(), one);

  // Join different positions
  frames = CalleeFrames{test::make_frame(
      test_kind_one,
      test::FrameProperties{.call_position = test_position_one})};
  frames.join_with(CalleeFrames{test::make_frame(
      test_kind_one,
      test::FrameProperties{.call_position = test_position_two})});
  EXPECT_EQ(
      frames,
      (CalleeFrames{
          test::make_frame(
              test_kind_one,
              test::FrameProperties{.call_position = test_position_one}),
          test::make_frame(
              test_kind_one,
              test::FrameProperties{.call_position = test_position_two})}));

  // Join same position, same kind, different frame properties.
  frames = CalleeFrames{test::make_frame(
      test_kind_one,
      test::FrameProperties{
          .call_position = test_position_one,
          .inferred_features = FeatureMayAlwaysSet{feature_one}})};
  frames.join_with(CalleeFrames{test::make_frame(
      test_kind_one,
      test::FrameProperties{
          .call_position = test_position_one,
          .inferred_features = FeatureMayAlwaysSet{feature_two}})});
  EXPECT_EQ(
      frames,
      (CalleeFrames{test::make_frame(
          test_kind_one,
          test::FrameProperties{
              .call_position = test_position_one,
              .inferred_features = FeatureMayAlwaysSet(
                  /* may */ FeatureSet{feature_one, feature_two},
                  /* always */ FeatureSet{}),
          })}));
}

TEST_F(CalleeFramesTest, Difference) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));

  auto* test_kind_one = context.kinds->get("TestSinkOne");
  auto* test_kind_two = context.kinds->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);
  auto* feature_one = context.features->get("FeatureOne");
  auto* feature_two = context.features->get("FeatureTwo");

  CalleeFrames frames, initial_frames;

  // Tests with empty left hand side.
  frames.difference_with(CalleeFrames{});
  EXPECT_TRUE(frames.is_bottom());

  frames.difference_with(CalleeFrames{
      test::make_frame(test_kind_one, test::FrameProperties{}),
  });
  EXPECT_TRUE(frames.is_bottom());

  initial_frames = CalleeFrames{
      test::make_frame(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
              .inferred_features = FeatureMayAlwaysSet{feature_one},
          }),
  };

  frames = initial_frames;
  frames.difference_with(CalleeFrames{});
  EXPECT_EQ(frames, initial_frames);

  frames = initial_frames;
  frames.difference_with(initial_frames);
  EXPECT_TRUE(frames.is_bottom());

  // Left hand side is bigger than right hand side in terms of the `Frame.leq`
  // operation.
  frames = initial_frames;
  frames.difference_with(CalleeFrames{
      test::make_frame(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
          }),
  });
  EXPECT_EQ(frames, initial_frames);

  // Left hand side is smaller than right hand side in terms of the `Frame.leq`
  // operation.
  frames = initial_frames;
  frames.difference_with(CalleeFrames{
      test::make_frame(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .origins = MethodSet{one},
          }),
  });
  EXPECT_TRUE(frames.is_bottom());

  // Left hand side and right hand side are incomparably different at the
  // `Frame` level (different features).
  frames = initial_frames;
  frames.difference_with(CalleeFrames{
      test::make_frame(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
              .inferred_features = FeatureMayAlwaysSet{feature_two},
          }),
  });
  EXPECT_EQ(frames, initial_frames);

  // Left hand side and right hand side have different positions.
  frames = initial_frames;
  frames.difference_with(CalleeFrames{
      test::make_frame(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_two,
          }),
  });
  EXPECT_EQ(frames, initial_frames);

  // Left hand side is smaller than right hand side (by one position).
  frames = CalleeFrames{
      test::make_frame(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
          }),
  };
  frames.difference_with(CalleeFrames{
      test::make_frame(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
          }),
      test::make_frame(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_two,
          }),
  });
  EXPECT_TRUE(frames.is_bottom());

  // Left hand side has more positions than right hand side.
  frames = CalleeFrames{
      test::make_frame(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
          }),
      test::make_frame(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_two,
          }),
  };
  frames.difference_with(CalleeFrames{test::make_frame(
      test_kind_one,
      test::FrameProperties{
          .callee = one,
          .call_position = test_position_one,
      })});
  EXPECT_EQ(
      frames,
      (CalleeFrames{
          test::make_frame(
              test_kind_one,
              test::FrameProperties{
                  .callee = one,
                  .call_position = test_position_two,
              }),
      }));

  // Left hand side is smaller for one position, and larger for another.
  frames = CalleeFrames{
      test::make_frame(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
          }),
      test::make_frame(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_two,
          }),
      test::make_frame(
          test_kind_two,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_two,
          }),
  };
  frames.difference_with(CalleeFrames{
      test::make_frame(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
          }),
      test::make_frame(
          test_kind_two,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
          }),
      test::make_frame(
          test_kind_two,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_two,
          })});
  EXPECT_EQ(
      frames,
      (CalleeFrames{test::make_frame(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_two,
          })}));

  // NOTE: Access path coverage in CallPositionFramesTest.cpp.
}

} // namespace marianatrench

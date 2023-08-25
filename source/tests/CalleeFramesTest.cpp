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

  auto* source_kind_one = context.kind_factory->get("TestSourceOne");
  auto* source_kind_two = context.kind_factory->get("TestSourceTwo");

  auto* position_one = context.positions->get(std::nullopt, 1);

  CalleeFrames frames;
  EXPECT_TRUE(frames.is_bottom());
  EXPECT_TRUE(frames.empty());
  EXPECT_EQ(frames.callee(), nullptr);

  frames.add(test::make_taint_config(source_kind_one, test::FrameProperties{}));
  EXPECT_FALSE(frames.is_bottom());
  EXPECT_EQ(frames.callee(), nullptr);
  EXPECT_EQ(
      frames,
      CalleeFrames{
          test::make_taint_config(source_kind_one, test::FrameProperties{})});

  // Add frame with the same position (nullptr), different kind
  frames.add(test::make_taint_config(
      source_kind_two, test::FrameProperties{.origins = MethodSet{one}}));
  EXPECT_EQ(
      frames,
      (CalleeFrames{
          test::make_taint_config(source_kind_one, test::FrameProperties{}),
          test::make_taint_config(
              source_kind_two,
              test::FrameProperties{.origins = MethodSet{one}}),
      }));

  // Add frame with a different position
  frames.add(test::make_taint_config(
      source_kind_two, test::FrameProperties{.call_position = position_one}));
  EXPECT_EQ(
      frames,
      (CalleeFrames{
          test::make_taint_config(source_kind_one, test::FrameProperties{}),
          test::make_taint_config(
              source_kind_two,
              test::FrameProperties{.origins = MethodSet{one}}),
          test::make_taint_config(
              source_kind_two,
              test::FrameProperties{.call_position = position_one}),
      }));
}

TEST_F(CalleeFramesTest, Leq) {
  auto context = test::make_empty_context();

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);

  // Comparison to bottom
  EXPECT_TRUE(CalleeFrames::bottom().leq(CalleeFrames::bottom()));
  EXPECT_TRUE(CalleeFrames::bottom().leq(CalleeFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{})}));
  EXPECT_FALSE((CalleeFrames{test::make_taint_config(
                    test_kind_one,
                    test::FrameProperties{.call_position = test_position_one})})
                   .leq(CalleeFrames::bottom()));

  // Comparison to self
  EXPECT_TRUE((CalleeFrames{test::make_taint_config(
                   test_kind_one, test::FrameProperties{})})
                  .leq(CalleeFrames{test::make_taint_config(
                      test_kind_one, test::FrameProperties{})}));

  // Same position, different kinds
  EXPECT_TRUE(
      (CalleeFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{.call_position = test_position_one})})
          .leq(CalleeFrames{
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{.call_position = test_position_one}),
              test::make_taint_config(
                  test_kind_two,
                  test::FrameProperties{.call_position = test_position_one}),
          }));
  EXPECT_FALSE(
      (CalleeFrames{
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{.call_position = test_position_one}),
           test::make_taint_config(
               test_kind_two,
               test::FrameProperties{.call_position = test_position_one}),
       })
          .leq(CalleeFrames{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{.call_position = test_position_one})}));

  // Different positions
  EXPECT_TRUE(
      (CalleeFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{.call_position = test_position_one})})
          .leq(CalleeFrames{
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{.call_position = test_position_one}),
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{.call_position = test_position_two}),
          }));
  EXPECT_FALSE(
      (CalleeFrames{
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{.call_position = test_position_one}),
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{.call_position = test_position_two})})
          .leq(CalleeFrames{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{.call_position = test_position_one})}));
}

TEST_F(CalleeFramesTest, Equals) {
  auto context = test::make_empty_context();

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);

  // Comparison to bottom
  EXPECT_TRUE(CalleeFrames::bottom().equals(CalleeFrames::bottom()));
  EXPECT_FALSE(
      CalleeFrames::bottom().equals(CalleeFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.call_position = test_position_one})}));
  EXPECT_FALSE((CalleeFrames{test::make_taint_config(
                    test_kind_one,
                    test::FrameProperties{.call_position = test_position_one})})
                   .equals(CalleeFrames::bottom()));

  // Comparison to self
  EXPECT_TRUE((CalleeFrames{test::make_taint_config(
                   test_kind_one, test::FrameProperties{})})
                  .equals(CalleeFrames{test::make_taint_config(
                      test_kind_one, test::FrameProperties{})}));

  // Different positions
  EXPECT_FALSE(
      (CalleeFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{.call_position = test_position_one})})
          .equals(CalleeFrames{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{.call_position = test_position_two})}));

  // Different kinds
  EXPECT_FALSE((CalleeFrames{test::make_taint_config(
                    test_kind_one, test::FrameProperties{})})
                   .equals(CalleeFrames{test::make_taint_config(
                       test_kind_two, test::FrameProperties{})}));
}

TEST_F(CalleeFramesTest, JoinWith) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);
  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");

  // Join with bottom
  EXPECT_EQ(
      CalleeFrames::bottom().join(CalleeFrames{
          test::make_taint_config(test_kind_one, test::FrameProperties{})}),
      CalleeFrames{
          test::make_taint_config(test_kind_one, test::FrameProperties{})});

  EXPECT_EQ(
      (CalleeFrames{
           test::make_taint_config(test_kind_one, test::FrameProperties{})})
          .join(CalleeFrames::bottom()),
      CalleeFrames{
          test::make_taint_config(test_kind_one, test::FrameProperties{})});

  auto frames = (CalleeFrames{test::make_taint_config(
                     test_kind_one,
                     test::FrameProperties{
                         .callee = one, .call_info = CallInfo::callsite()})})
                    .join(CalleeFrames::bottom());
  EXPECT_EQ(
      frames,
      CalleeFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one, .call_info = CallInfo::callsite()})});
  EXPECT_EQ(frames.callee(), one);

  frames = CalleeFrames::bottom().join(CalleeFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee = one, .call_info = CallInfo::callsite()})});
  EXPECT_EQ(
      frames,
      CalleeFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one, .call_info = CallInfo::callsite()})});
  EXPECT_EQ(frames.callee(), one);

  // Join different positions
  frames = CalleeFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{.call_position = test_position_one})};
  frames.join_with(CalleeFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{.call_position = test_position_two})});
  EXPECT_EQ(
      frames,
      (CalleeFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{.call_position = test_position_one}),
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{.call_position = test_position_two})}));

  // Join same position, same kind, different frame properties.
  frames = CalleeFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .call_position = test_position_one,
          .inferred_features = FeatureMayAlwaysSet{feature_one}})};
  frames.join_with(CalleeFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .call_position = test_position_one,
          .inferred_features = FeatureMayAlwaysSet{feature_two}})});
  EXPECT_EQ(
      frames,
      (CalleeFrames{test::make_taint_config(
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

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);
  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");

  CalleeFrames frames, initial_frames;

  // Tests with empty left hand side.
  frames.difference_with(CalleeFrames{});
  EXPECT_TRUE(frames.is_bottom());

  frames.difference_with(CalleeFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
  });
  EXPECT_TRUE(frames.is_bottom());

  initial_frames = CalleeFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .call_info = CallInfo::callsite(),
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
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
              .call_info = CallInfo::callsite(),
          }),
  });
  EXPECT_EQ(frames, initial_frames);

  // Left hand side is smaller than right hand side in terms of the `Frame.leq`
  // operation.
  frames = initial_frames;
  frames.difference_with(CalleeFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .call_info = CallInfo::callsite(),
          }),
  });
  EXPECT_TRUE(frames.is_bottom());

  // Left hand side and right hand side are incomparably different at the
  // `Frame` level (different features).
  frames = initial_frames;
  frames.difference_with(CalleeFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .call_info = CallInfo::callsite(),
          }),
  });
  EXPECT_EQ(frames, initial_frames);

  // Left hand side and right hand side have different positions.
  frames = initial_frames;
  frames.difference_with(CalleeFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_two,
              .call_info = CallInfo::callsite(),
          }),
  });
  EXPECT_EQ(frames, initial_frames);

  // Left hand side is smaller than right hand side (by one position).
  frames = CalleeFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
              .call_info = CallInfo::callsite(),
          }),
  };
  frames.difference_with(CalleeFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
              .call_info = CallInfo::callsite(),
          }),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_two,
              .call_info = CallInfo::callsite(),
          }),
  });
  EXPECT_TRUE(frames.is_bottom());

  // Left hand side has more positions than right hand side.
  frames = CalleeFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
              .call_info = CallInfo::callsite(),
          }),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_two,
              .call_info = CallInfo::callsite(),
          }),
  };
  frames.difference_with(CalleeFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .callee = one,
          .call_position = test_position_one,
          .call_info = CallInfo::callsite(),
      })});
  EXPECT_EQ(
      frames,
      (CalleeFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee = one,
                  .call_position = test_position_two,
                  .call_info = CallInfo::callsite(),
              }),
      }));

  // Left hand side is smaller for one position, and larger for another.
  frames = CalleeFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
              .call_info = CallInfo::callsite(),
          }),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_two,
              .call_info = CallInfo::callsite(),
          }),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_two,
              .call_info = CallInfo::callsite(),
          }),
  };
  frames.difference_with(CalleeFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
              .call_info = CallInfo::callsite(),
          }),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
              .call_info = CallInfo::callsite(),
          }),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_two,
              .call_info = CallInfo::callsite(),
          })});
  EXPECT_EQ(
      frames,
      (CalleeFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_two,
              .call_info = CallInfo::callsite(),
          })}));

  // NOTE: Access path coverage in CallPositionFramesTest.cpp.
}

TEST_F(CalleeFramesTest, Iterator) {
  auto context = test::make_empty_context();

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);

  auto call_position_frames = CalleeFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.call_position = test_position_one}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.call_position = test_position_two}),
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
              test::FrameProperties{.call_position = test_position_one})),
      frames.end());
  EXPECT_NE(
      std::find(
          frames.begin(),
          frames.end(),
          test::make_taint_frame(
              test_kind_one,
              test::FrameProperties{.call_position = test_position_two})),
      frames.end());
  EXPECT_NE(
      std::find(
          frames.begin(),
          frames.end(),
          test::make_taint_frame(test_kind_two, test::FrameProperties{})),
      frames.end());
}

TEST_F(CalleeFramesTest, Map) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* test_kind = context.kind_factory->get("TestSink");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);
  auto* feature_one = context.feature_factory->get("FeatureOne");

  auto frames = CalleeFrames{
      test::make_taint_config(
          test_kind,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_one,
              .call_info = CallInfo::callsite(),
          }),
      test::make_taint_config(
          test_kind,
          test::FrameProperties{
              .callee = one,
              .call_position = test_position_two,
              .call_info = CallInfo::callsite(),
          }),
  };
  frames.map([feature_one](Frame frame) {
    frame.add_inferred_features(FeatureMayAlwaysSet{feature_one});
    return frame;
  });
  EXPECT_EQ(
      frames,
      (CalleeFrames{
          test::make_taint_config(
              test_kind,
              test::FrameProperties{
                  .callee = one,
                  .call_position = test_position_one,
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .call_info = CallInfo::callsite(),
              }),
          test::make_taint_config(
              test_kind,
              test::FrameProperties{
                  .callee = one,
                  .call_position = test_position_two,
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .call_info = CallInfo::callsite(),
              }),
      }));
}

TEST_F(CalleeFramesTest, FeaturesAndPositions) {
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
  auto frames = CalleeFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .locally_inferred_features = FeatureMayAlwaysSet(
                  /* may */ FeatureSet{feature_one},
                  /* always */ FeatureSet{})}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{.call_position = test_position_one})};
  frames.add_locally_inferred_features(FeatureMayAlwaysSet{feature_two});
  EXPECT_EQ(
      frames.locally_inferred_features(
          /* position */ test_position_one,
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf))),
      FeatureMayAlwaysSet{feature_two});
  EXPECT_EQ(
      frames.locally_inferred_features(
          /* position */ nullptr,
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf))),
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{feature_one},
          /* always */ FeatureSet{feature_two}));
  EXPECT_EQ(
      frames.locally_inferred_features(
          /* position */ nullptr,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument))),
      FeatureMayAlwaysSet::bottom());

  // Test add_local_positions()
  frames = CalleeFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .call_position = test_position_one,
          }),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .call_position = test_position_two,
          }),
  };
  EXPECT_EQ(frames.local_positions(), LocalPositionSet{});
  frames.add_local_position(test_position_one);
  EXPECT_EQ(frames.local_positions(), LocalPositionSet{test_position_one});

  // Test set_local_positions()
  frames.set_local_positions(LocalPositionSet{test_position_two});
  EXPECT_EQ(frames.local_positions(), LocalPositionSet{test_position_two});
}

TEST_F(CalleeFramesTest, Propagate) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* test_position_one = context.positions->get("Test.java", 1);
  auto* test_position_two = context.positions->get("Test.java", 2);
  auto* feature_one = context.feature_factory->get("FeatureOne");

  /**
   * The following `CalleeFrames` looks like (callee == nullptr):
   *
   * position_one  -> kind_one -> Frame(port=arg(0), distance=1,
   *                                    local_features=Always(feature_one))
   *                  kind_two -> Frame(port=arg(0), distance=1)
   */
  auto non_crtex_frames = CalleeFrames{
      /* call_position == test_position_one */
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .call_position = test_position_one,
              .distance = 1,
              .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
              .call_info = CallInfo::callsite()}),
      /* call_position == nullptr */
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .distance = 1,
              .call_info = CallInfo::callsite()}),
  };
  /**
   * After propagating with
   *   (callee=two, callee_port=arg(1), call_position=test_position_two):
   *
   * CalleeFrames: (callee == one)
   * position_two -> kind_one -> Frame(port=arg(1), distance=2)
   *                                   inferred_feature=May(feature_one))
   *                 kind_two -> Frame(port=arg(1), distance=2)
   *
   * Intuition:
   * `frames.propagate(one, arg(1), test_position_two)` is called when we see a
   * callsite like `one(arg0, arg1)`, and are processing the models for arg1 at
   * that callsite (which is at test_position_two).
   *
   * The callee, position, and ports after `propagate` should be what is passed
   * to propagate.
   *
   * For each kind in the original `frames`, the propagated frame should have
   * distance = min(all_distances_for_that_kind) + 1, with the exception of
   * "Anchor" frames which always have distance = 0.
   *
   * Locally inferred features are explicitly set to `bottom()` because these
   * should be propagated into inferred features (joined across each kind).
   */
  auto expected_instantiated_name =
      CanonicalName(CanonicalName::InstantiatedValue{one->signature()});
  EXPECT_EQ(
      non_crtex_frames.propagate(
          /* callee */ one,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* call_position */ test_position_two,
          /* maximum_source_sink_distance */ 100,
          context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {},
          CallClassIntervalContext(),
          ClassIntervals::Interval::top()),
      (CalleeFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .callee = one,
                  .call_position = test_position_two,
                  .distance = 2,
                  .inferred_features = FeatureMayAlwaysSet(
                      /* may */ FeatureSet{feature_one},
                      /* always */ FeatureSet{feature_one}),
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .call_info = CallInfo::callsite()}),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .callee = one,
                  .call_position = test_position_two,
                  .distance = 2,
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .call_info = CallInfo::callsite()}),
      }));
  /**
   * The following `CalleeFrames` looks like (callee == nullptr):
   *
   * position_one  -> kind_one -> Frame(port=anchor, distance=0)
   * null position -> Frame(port=anchor, distance=0)
   *
   * NOTE: Realistically, we wouldn't normally have frames with distance > 0 if
   * callee == nullptr. However, we need callee == nullptr to test the "Anchor"
   * port scenarios (otherwise they are ignored and treated as regular ports).
   */
  auto crtex_frames = CalleeFrames{
      /* call_position == test_position_one */
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Anchor)),
              .call_position = test_position_one,
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"%programmatic_leaf_name%"})},
              .call_info = CallInfo::origin()}),
      /* call_position == nullptr */
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Anchor)),
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"%programmatic_leaf_name%"})},

              .call_info = CallInfo::origin()}),
  };

  /**
   * After propagating with
   *   (callee=two, callee_port=arg(1), call_position=test_position_two):
   *
   * CalleeFrames: (callee == one)
   * position_two -> kind_one -> Frame(port=anchor:arg(0), distance=0)
   *                 kind_two -> Frame(port=anchor:arg(0), distance=0)
   *
   * Intuition:
   * `frames.propagate(one, arg(1), test_position_two)` is called when we see a
   * callsite like `one(arg0, arg1)`, and are processing the models for arg1 at
   * that callsite (which is at test_position_two).
   *
   * "Anchor" frames behave slightly differently, in that the port
   * is "canonicalized" such that the `this` parameter has index arg(-1) for
   * non-static methods, and the first parameter starts at index arg(0).
   *
   * For each kind in the original `frames`, the propagated frame should have
   * distance = min(all_distances_for_that_kind) + 1, with the exception of
   * "Anchor" frames which always have distance = 0.
   *
   * Locally inferred features are explicitly set to `bottom()` because these
   * should be propagated into inferred features (joined across each kind).
   */
  EXPECT_EQ(
      crtex_frames.propagate(
          /* callee */ one,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* call_position */ test_position_two,
          /* maximum_source_sink_distance */ 100,
          context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {},
          CallClassIntervalContext(),
          ClassIntervals::Interval::top()),
      (CalleeFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(
                      Root(Root::Kind::Anchor),
                      Path{PathElement::field("Argument(0)")}),
                  .callee = one,
                  .call_position = test_position_two,
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
                      Path{PathElement::field("Argument(0)")}),
                  .callee = one,
                  .call_position = test_position_two,
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .canonical_names =
                      CanonicalNameSetAbstractDomain{
                          expected_instantiated_name},
                  .call_info = CallInfo::callsite()}),
      }));
}

TEST_F(CalleeFramesTest, AttachPosition) {
  auto context = test::make_empty_context();

  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");
  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* test_kind_two = context.kind_factory->get("TestSinkTwo");
  auto* test_position_one = context.positions->get(std::nullopt, 1);
  auto* test_position_two = context.positions->get(std::nullopt, 2);
  auto* test_position_three = context.positions->get(std::nullopt, 3);

  auto frames = CalleeFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .call_position = test_position_one,
              .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{feature_two}}),
      // Will be merged with the frame above after attach_position because they
      // have the same kind. Features will be joined too.
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.call_position = test_position_two}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{.call_position = test_position_two}),
  };

  auto frames_with_new_position = frames.attach_position(test_position_three);

  EXPECT_EQ(
      frames_with_new_position,
      (CalleeFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .call_position = test_position_three,
                  .inferred_features = FeatureMayAlwaysSet(
                      /* may */ FeatureSet{feature_one, feature_two},
                      /* always */ FeatureSet{}),
                  .locally_inferred_features = FeatureMayAlwaysSet{feature_two},
                  .call_info = CallInfo::origin(),
              }),
          test::make_taint_config(
              test_kind_two,
              test::FrameProperties{
                  .call_position = test_position_three,
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom(),
                  .call_info = CallInfo::origin(),
              }),
      }));
}

} // namespace marianatrench

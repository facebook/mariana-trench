/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>
#include <gmock/gmock.h>

#include <mariana-trench/KindFrames.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class KindFramesTest : public test::Test {};

TEST_F(KindFramesTest, Add) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));

  auto* source_kind_one = context.kind_factory->get("TestSourceOne");
  auto interval = CallClassIntervalContext(
      ClassIntervals::Interval::finite(2, 3),
      /* preserves_type_context */ false);

  KindFrames frames;
  EXPECT_TRUE(frames.is_bottom());
  EXPECT_EQ(frames.kind(), nullptr);

  // Add frame with default interval
  frames.add(test::make_taint_config(source_kind_one, test::FrameProperties{}));
  EXPECT_FALSE(frames.is_bottom());
  EXPECT_EQ(frames.kind(), source_kind_one);
  EXPECT_EQ(
      frames,
      KindFrames{
          test::make_taint_config(source_kind_one, test::FrameProperties{})});

  // Add frame with more details (origins)
  frames.add(test::make_taint_config(
      source_kind_one, test::FrameProperties{.origins = MethodSet{one}}));
  EXPECT_EQ(
      frames,
      (KindFrames{
          test::make_taint_config(
              source_kind_one,
              test::FrameProperties{.origins = MethodSet{one}}),
      }));
  EXPECT_EQ(1, std::count_if(frames.begin(), frames.end(), [](auto) {
              return true;
            }));

  // Add frame with a different interval
  frames.add(test::make_taint_config(
      source_kind_one,
      test::FrameProperties{.class_interval_context = interval}));
  EXPECT_EQ(
      frames,
      (KindFrames{
          test::make_taint_config(
              source_kind_one,
              test::FrameProperties{.origins = MethodSet{one}}),
          test::make_taint_config(
              source_kind_one,
              test::FrameProperties{.class_interval_context = interval}),
      }));
  EXPECT_EQ(2, std::count_if(frames.begin(), frames.end(), [](auto) {
              return true;
            }));
}

TEST_F(KindFramesTest, Leq) {
  auto context = test::make_empty_context();

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto interval_one = CallClassIntervalContext(
      ClassIntervals::Interval::finite(2, 3),
      /* preserves_type_context */ false);
  auto interval_one_preserves_type_context = CallClassIntervalContext(
      ClassIntervals::Interval::finite(2, 3),
      /* preserves_type_context */ true);
  auto interval_two = CallClassIntervalContext(
      ClassIntervals::Interval::finite(4, 5),
      /* preserves_type_context */ false);
  auto interval_two_preserves_type_context = CallClassIntervalContext(
      ClassIntervals::Interval::finite(4, 5),
      /* preserves_type_context */ true);
  auto interval_three = CallClassIntervalContext(
      ClassIntervals::Interval::top(),
      /* preserves_type_context */ true);
  auto* feature_one = context.feature_factory->get("FeatureOne");

  // Comparison to bottom
  EXPECT_TRUE(KindFrames::bottom().leq(KindFrames::bottom()));
  EXPECT_TRUE(KindFrames::bottom().leq(KindFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{})}));
  EXPECT_FALSE(
      (KindFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{.class_interval_context = interval_one})})
          .leq(KindFrames::bottom()));
  EXPECT_FALSE(
      (KindFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{.class_interval_context = interval_three})})
          .leq(KindFrames::bottom()));

  // Comparison to self
  EXPECT_TRUE((KindFrames{test::make_taint_config(
                   test_kind_one, test::FrameProperties{})})
                  .leq(KindFrames{test::make_taint_config(
                      test_kind_one, test::FrameProperties{})}));

  // Different intervals
  EXPECT_TRUE(
      (KindFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{.class_interval_context = interval_one})})
          .leq(KindFrames{
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{
                      .class_interval_context = interval_one}),
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{
                      .class_interval_context = interval_two}),
          }));
  EXPECT_FALSE(
      (KindFrames{
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{.class_interval_context = interval_one}),
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{.class_interval_context = interval_two}),
       })
          .leq(KindFrames{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{.class_interval_context = interval_one})}));

  // Different intervals (preserves_type_context)
  EXPECT_TRUE(
      (KindFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{
               .class_interval_context = interval_one_preserves_type_context})})
          .leq(KindFrames{
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{
                      .class_interval_context = interval_one}),
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{
                      .class_interval_context =
                          interval_one_preserves_type_context}),
          }));
  EXPECT_FALSE(
      (KindFrames{
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{.class_interval_context = interval_one}),
           test::make_taint_config(
               test_kind_one,
               test::FrameProperties{
                   .class_interval_context =
                       interval_two_preserves_type_context})})
          .leq(KindFrames{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{.class_interval_context = interval_one})}));

  // Same intervals, different frame details
  EXPECT_TRUE(
      (KindFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{.class_interval_context = interval_one})})
          .leq(KindFrames{
              test::make_taint_config(
                  test_kind_one,
                  test::FrameProperties{
                      .class_interval_context = interval_one,
                      .user_features = FeatureSet{feature_one}}),
          }));
  EXPECT_FALSE((KindFrames{test::make_taint_config(
                    test_kind_one,
                    test::FrameProperties{
                        .class_interval_context = interval_one,
                        .user_features = FeatureSet{feature_one}})})
                   .leq(KindFrames{
                       test::make_taint_config(
                           test_kind_one,
                           test::FrameProperties{
                               .class_interval_context = interval_one}),
                   }));
}

TEST_F(KindFramesTest, Equals) {
  auto context = test::make_empty_context();

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto interval_one = CallClassIntervalContext(
      ClassIntervals::Interval::finite(2, 3),
      /* preserves_type_context */ false);
  auto interval_two = CallClassIntervalContext(
      ClassIntervals::Interval::finite(4, 5),
      /* preserves_type_context */ false);

  // Comparison to bottom
  EXPECT_TRUE(KindFrames::bottom().equals(KindFrames::bottom()));
  EXPECT_FALSE(KindFrames::bottom().equals(KindFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{.class_interval_context = interval_one})}));
  EXPECT_FALSE(
      (KindFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{.class_interval_context = interval_one})})
          .equals(KindFrames::bottom()));

  // Comparison to self
  EXPECT_TRUE((KindFrames{test::make_taint_config(
                   test_kind_one, test::FrameProperties{})})
                  .equals(KindFrames{test::make_taint_config(
                      test_kind_one, test::FrameProperties{})}));

  // Different intervals
  EXPECT_FALSE(
      (KindFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{.class_interval_context = interval_one})})
          .equals(KindFrames{test::make_taint_config(
              test_kind_one,
              test::FrameProperties{.class_interval_context = interval_two})}));

  // Different intervals (preserves_type_context)
  EXPECT_FALSE((KindFrames{test::make_taint_config(
                    test_kind_one,
                    test::FrameProperties{
                        .class_interval_context = CallClassIntervalContext(
                            ClassIntervals::Interval::top(),
                            /* preserves_type_context */ true)})})
                   .equals(KindFrames{test::make_taint_config(
                       test_kind_one,
                       test::FrameProperties{
                           .class_interval_context = CallClassIntervalContext(
                               ClassIntervals::Interval::top(),
                               /* preserves_type_context */ false)})}));
}

TEST_F(KindFramesTest, JoinWith) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto interval_one = CallClassIntervalContext(
      ClassIntervals::Interval::finite(2, 3),
      /* preserves_type_context */ false);
  auto interval_two = CallClassIntervalContext(
      ClassIntervals::Interval::finite(4, 5),
      /* preserves_type_context */ false);
  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");

  // Join with bottom
  EXPECT_EQ(
      KindFrames::bottom().join(KindFrames{
          test::make_taint_config(test_kind_one, test::FrameProperties{})}),
      KindFrames{
          test::make_taint_config(test_kind_one, test::FrameProperties{})});

  EXPECT_EQ(
      (KindFrames{
           test::make_taint_config(test_kind_one, test::FrameProperties{})})
          .join(KindFrames::bottom()),
      KindFrames{
          test::make_taint_config(test_kind_one, test::FrameProperties{})});

  auto frames =
      (KindFrames{test::make_taint_config(
           test_kind_one,
           test::FrameProperties{.class_interval_context = interval_one})})
          .join(KindFrames::bottom());
  EXPECT_EQ(frames.kind(), test_kind_one);

  // Join different intervals
  frames = KindFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{.class_interval_context = interval_one})};
  frames.join_with(KindFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{.class_interval_context = interval_two})});
  EXPECT_EQ(
      frames,
      (KindFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{.class_interval_context = interval_one}),
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{.class_interval_context = interval_two})}));

  // Join same interval, different frame properties.
  frames = KindFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .class_interval_context = interval_one,
          .inferred_features = FeatureMayAlwaysSet{feature_one}})};
  frames.join_with(KindFrames{test::make_taint_config(
      test_kind_one,
      test::FrameProperties{
          .class_interval_context = interval_one,
          .inferred_features = FeatureMayAlwaysSet{feature_two}})});
  EXPECT_EQ(
      frames,
      (KindFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .class_interval_context = interval_one,
              .inferred_features = FeatureMayAlwaysSet(
                  /* may */ FeatureSet{feature_one, feature_two},
                  /* always */ FeatureSet{}),
          })}));
}

TEST_F(KindFramesTest, Difference) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto interval_one = CallClassIntervalContext(
      ClassIntervals::Interval::finite(2, 3),
      /* preserves_type_context */ false);
  auto interval_two = CallClassIntervalContext(
      ClassIntervals::Interval::finite(4, 5),
      /* preserves_type_context */ false);
  auto* feature_one = context.feature_factory->get("FeatureOne");

  KindFrames frames;

  // Tests with empty left hand side.
  frames.difference_with(KindFrames{});
  EXPECT_TRUE(frames.is_bottom());

  frames.difference_with(KindFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
  });
  EXPECT_TRUE(frames.is_bottom());

  const auto initial_frames = KindFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.class_interval_context = interval_one}),
  };

  frames = initial_frames;
  frames.difference_with(KindFrames{});
  EXPECT_EQ(frames, initial_frames);

  frames = initial_frames;
  frames.difference_with(initial_frames);
  EXPECT_TRUE(frames.is_bottom());

  // Left hand side has more intervals than right hand side.
  auto two_interval_frames = KindFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.class_interval_context = interval_one}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.class_interval_context = interval_two}),
  };
  two_interval_frames.difference_with(initial_frames);
  EXPECT_EQ(
      two_interval_frames,
      (KindFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{.class_interval_context = interval_two}),
      }));

  // Left hand side has a larger `Frame`.
  auto larger_initial_frames = KindFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .class_interval_context = interval_one,
              .user_features = FeatureSet{feature_one}}),
  };
  frames = larger_initial_frames;
  frames.difference_with(initial_frames);
  EXPECT_EQ(frames, larger_initial_frames);

  // Left hand side has fewer intervals than right hand side.
  frames = initial_frames;
  frames.difference_with(KindFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.class_interval_context = interval_one}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.class_interval_context = interval_two}),
  });
  EXPECT_TRUE(frames.is_bottom());

  // Left hand side has a smaller `Frame`.
  frames = initial_frames;
  frames.difference_with(KindFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .class_interval_context = interval_one,
              .user_features = FeatureSet{feature_one}}),
  });
  EXPECT_EQ(frames, KindFrames{});
  EXPECT_TRUE(frames.is_bottom());

  // Left hand side and right hand side have incomparable intervals.
  frames = initial_frames;
  frames.difference_with(KindFrames{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
  });
  EXPECT_EQ(frames, initial_frames);

  // Left hand side is smaller for one interval, and larger for another.
  frames = KindFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .class_interval_context = interval_one,
              .user_features = FeatureSet{feature_one},
          }),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .class_interval_context = interval_two,
          }),
  };
  frames.difference_with(KindFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .class_interval_context = interval_one,
          }),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .class_interval_context = interval_one,
              .user_features = FeatureSet{feature_one},
          }),
  });
  EXPECT_EQ(
      frames,
      (KindFrames{test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.class_interval_context = interval_two})}));
}

TEST_F(KindFramesTest, Iterator) {
  auto context = test::make_empty_context();

  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto interval_one = CallClassIntervalContext(
      ClassIntervals::Interval::finite(2, 3),
      /* preserves_type_context */ false);
  auto interval_two = CallClassIntervalContext(
      ClassIntervals::Interval::finite(4, 5),
      /* preserves_type_context */ false);

  auto kind_frames = KindFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.class_interval_context = interval_one}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{.class_interval_context = interval_two}),
  };

  std::vector<Frame> frames;
  for (const auto& frame : kind_frames) {
    frames.push_back(frame);
  }

  EXPECT_EQ(frames.size(), 2);
  EXPECT_NE(
      std::find(
          frames.begin(),
          frames.end(),
          test::make_taint_frame(
              test_kind_one,
              test::FrameProperties{.class_interval_context = interval_one})),
      frames.end());
  EXPECT_NE(
      std::find(
          frames.begin(),
          frames.end(),
          test::make_taint_frame(
              test_kind_one,
              test::FrameProperties{.class_interval_context = interval_two})),
      frames.end());
}

TEST_F(KindFramesTest, Map) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* test_kind = context.kind_factory->get("TestSink");
  auto interval_one = CallClassIntervalContext(
      ClassIntervals::Interval::finite(2, 3),
      /* preserves_type_context */ false);
  auto interval_two = CallClassIntervalContext(
      ClassIntervals::Interval::finite(4, 5),
      /* preserves_type_context */ false);
  auto* feature_one = context.feature_factory->get("FeatureOne");

  auto frames = KindFrames{
      test::make_taint_config(
          test_kind,
          test::FrameProperties{
              .class_interval_context = interval_one,
          }),
      test::make_taint_config(
          test_kind,
          test::FrameProperties{
              .class_interval_context = interval_two,
          }),
  };
  frames.map([feature_one](Frame frame) {
    frame.add_inferred_features(FeatureMayAlwaysSet{feature_one});
    return frame;
  });
  EXPECT_EQ(
      frames,
      (KindFrames{
          test::make_taint_config(
              test_kind,
              test::FrameProperties{
                  .class_interval_context = interval_one,
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
              }),
          test::make_taint_config(
              test_kind,
              test::FrameProperties{
                  .class_interval_context = interval_two,
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
              }),
      }));
}

TEST_F(KindFramesTest, Filter) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* test_kind = context.kind_factory->get("TestSink");
  auto interval_one = CallClassIntervalContext(
      ClassIntervals::Interval::finite(2, 3),
      /* preserves_type_context */ false);
  auto interval_two = CallClassIntervalContext(
      ClassIntervals::Interval::finite(4, 5),
      /* preserves_type_context */ false);

  auto frames = KindFrames{
      test::make_taint_config(
          test_kind,
          test::FrameProperties{
              .class_interval_context = interval_one,
          }),
      test::make_taint_config(
          test_kind,
          test::FrameProperties{
              .class_interval_context = interval_two,
          }),
  };
  frames.filter([&interval_one](const Frame& frame) {
    return frame.class_interval_context() == interval_one;
  });
  EXPECT_EQ(
      frames,
      (KindFrames{
          test::make_taint_config(
              test_kind,
              test::FrameProperties{
                  .class_interval_context = interval_one,
              }),
      }));

  // Filter everything.
  frames.filter([](const auto&) { return false; });
  EXPECT_TRUE(frames.is_bottom());
}

TEST_F(KindFramesTest, Propagate) {
  auto context = test::make_empty_context();
  context.options = test::make_default_options();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));
  auto interval_one = CallClassIntervalContext(
      ClassIntervals::Interval::finite(2, 3),
      /* preserves_type_context */ false);
  auto interval_two = CallClassIntervalContext(
      ClassIntervals::Interval::finite(4, 5),
      /* preserves_type_context */ false);
  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* call_position = context.positions->get("Test.java", 1);

  // Test propagating non-crtex frames (crtex-ness to be determined by caller,
  // typically using the callee_port).
  auto non_crtex_frames = KindFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .class_interval_context = interval_one,
              .distance = 1,
              .origins = MethodSet{one},
              .call_info = CallInfo::callsite()}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee = one,
              .class_interval_context = interval_two,
              .distance = 2,
              .origins = MethodSet{one},
              .call_info = CallInfo::callsite()}),
  };
  std::vector<const Feature*> via_type_of_features_added;

  // The callee interval is default (top, !preserves_type_context) in some of
  // the following situations:
  //  - The receiver's type is unknown
  //  - It is not an invoke_virtual call (e.g. static method call)
  // The expected behavior is that the propagation works as if class intervals
  // didn't exist.
  EXPECT_EQ(
      non_crtex_frames.propagate(
          /* callee */ two,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          call_position,
          /* locally_inferred_features */ FeatureMayAlwaysSet{feature_one},
          /* maximum_source_sink_distance */ 100,
          context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {},
          via_type_of_features_added,
          /* use default type context */ CallClassIntervalContext(),
          /* caller_class_interval */ ClassIntervals::Interval::top()),
      (KindFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = call_position,
                  .distance = 2,
                  .origins = MethodSet{one},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .call_info = CallInfo::callsite()}),
      }));
}

TEST_F(KindFramesTest, PropagateIntervals) {
  auto context = test::make_empty_context();

  // Make sure class intervals are enabled for this.
  context.options = std::make_unique<Options>(
      /* models_paths */ std::vector<std::string>{},
      /* field_models_path */ std::vector<std::string>{},
      /* literal_models_path */ std::vector<std::string>{},
      /* rules_paths */ std::vector<std::string>{},
      /* lifecycles_paths */ std::vector<std::string>{},
      /* shims_path */ std::vector<std::string>{},
      /* graphql_metadata_paths */ std::string{},
      /* proguard_configuration_paths */ std::vector<std::string>{},
      /* sequential */ false,
      /* skip_source_indexing */ true,
      /* skip_analysis */ true,
      /* model_generators_configuration */
      std::vector<ModelGeneratorConfiguration>{},
      /* model_generators_search_path */ std::vector<std::string>{},
      /* remove_unreachable_code */ false,
      /* emit_all_via_cast_features */ false,
      /* source_root_directory */ ".",
      /* enable_cross_component_analysis */ false,
      /* enable_class_intervals */ true);

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));
  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* call_position = context.positions->get("Test.java", 1);

  auto caller_class_interval = ClassIntervals::Interval::finite(6, 7);
  auto callee_port = AccessPath(Root(Root::Kind::Argument, 0));
  std::vector<const Feature*> via_type_of_features_added;

  auto interval_2_3_t = CallClassIntervalContext(
      ClassIntervals::Interval::finite(2, 3),
      /* preserves_type_context */ true);
  auto interval_2_3_f = CallClassIntervalContext(
      ClassIntervals::Interval::finite(2, 3),
      /* preserves_type_context */ false);
  auto interval_5_6_t = CallClassIntervalContext(
      ClassIntervals::Interval::finite(5, 6),
      /* preserves_type_context */ true);
  auto interval_5_6_f = CallClassIntervalContext(
      ClassIntervals::Interval::finite(5, 6),
      /* preserves_type_context */ false);
  auto interval_1_4_t = CallClassIntervalContext(
      ClassIntervals::Interval::finite(1, 4),
      /* preserves_type_context */ true);
  auto interval_1_4_f = CallClassIntervalContext(
      ClassIntervals::Interval::finite(1, 4),
      /* preserves_type_context */ false);

  {
    auto frames = KindFrames{
        // Declaration frames should be propagated as (caller_class_interval,
        // preserves_type_context=true). Note that declaration frames should
        // never have a non-default callee interval because intervals cannot
        // be user-defined.
        test::make_taint_config(
            test_kind_one,
            test::FrameProperties{
                .callee = nullptr,
                .class_interval_context = CallClassIntervalContext(),
                .distance = 0,
                .call_info = CallInfo::declaration()}),
    };
    EXPECT_EQ(
        frames.propagate(
            /* callee */ two,
            callee_port,
            call_position,
            /* locally_inferred_features */ FeatureMayAlwaysSet{},
            /* maximum_source_sink_distance */ 100,
            context,
            /* source_register_types */ {},
            /* source_constant_arguments */ {},
            via_type_of_features_added,
            /* class_interval_context */ interval_1_4_f,
            caller_class_interval),
        (KindFrames{
            test::make_taint_config(
                test_kind_one,
                test::FrameProperties{
                    .callee_port = callee_port,
                    .callee = nullptr,
                    .call_position = call_position,
                    .class_interval_context = CallClassIntervalContext(
                        caller_class_interval,
                        /* preserves_type_context */ true),
                    .distance = 0,
                    .call_info = CallInfo::origin()}),
        }));
  }

  {
    auto frames = KindFrames{
        // When preserves_type_context=true, the frame's interval should be
        // intersected with the class_interval_context to get the final
        // propagated
        // interval.
        test::make_taint_config(
            test_kind_one,
            test::FrameProperties{
                .callee = one,
                .class_interval_context = interval_2_3_t,
                .distance = 1,
                .call_info = CallInfo::callsite()}),
        // When preserves_type_context=false for non-Declaration frames, it
        // should be propagated as if class intervals didn't exist, even if
        // the intervals do not intersect. Other properties of the propagated
        // frame (e.g. distance) should be joined.
        test::make_taint_config(
            test_kind_one,
            test::FrameProperties{
                .callee = one,
                .class_interval_context = interval_5_6_f,
                .distance = 4,
                .call_info = CallInfo::callsite()}),
        test::make_taint_config(
            test_kind_one,
            test::FrameProperties{
                .callee = one,
                .class_interval_context = interval_2_3_f,
                .distance = 3,
                .call_info = CallInfo::callsite()}),
    };
    EXPECT_EQ(
        frames.propagate(
            /* callee */ two,
            callee_port,
            call_position,
            /* locally_inferred_features */ FeatureMayAlwaysSet{},
            /* maximum_source_sink_distance */ 100,
            context,
            /* source_register_types */ {},
            /* source_constant_arguments */ {},
            via_type_of_features_added,
            /* class_interval_context */ interval_1_4_f,
            caller_class_interval),
        (KindFrames{
            test::make_taint_config(
                test_kind_one,
                test::FrameProperties{
                    .callee_port = callee_port,
                    .callee = two,
                    .call_position = call_position,
                    .class_interval_context = interval_2_3_f,
                    .distance = 2,
                    .call_info = CallInfo::callsite()}),
            test::make_taint_config(
                test_kind_one,
                test::FrameProperties{
                    .callee_port = callee_port,
                    .callee = two,
                    .call_position = call_position,
                    .class_interval_context = interval_1_4_f,
                    .distance = 4,
                    .call_info = CallInfo::callsite()}),

        }));
  }

  {
    auto frames = KindFrames{
        // When preserves_type_context=true, only frames that intersect with the
        // class_interval_context should be propagated.
        test::make_taint_config(
            test_kind_one,
            test::FrameProperties{
                .callee = nullptr,
                .class_interval_context = interval_2_3_t,
                .distance = 1,
                .call_info = CallInfo::origin()}),
        // This frame will not intersect with class_interval_context, the
        // propagated frame will not have "origins" as a result.
        test::make_taint_config(
            test_kind_one,
            test::FrameProperties{
                .callee = nullptr,
                .class_interval_context = interval_5_6_t,
                .distance = 1,
                .origins = MethodSet{one},
                .call_info = CallInfo::origin()}),
        // This frame does not preserves type context and should be kept as is.
        test::make_taint_config(
            test_kind_one,
            test::FrameProperties{
                .callee = nullptr,
                .class_interval_context = interval_5_6_f,
                .distance = 1,
                .origins = MethodSet{two},
                .call_info = CallInfo::origin()}),
    };
    EXPECT_EQ(
        frames.propagate(
            /* callee */ two,
            callee_port,
            call_position,
            /* locally_inferred_features */ FeatureMayAlwaysSet{},
            /* maximum_source_sink_distance */ 100,
            context,
            /* source_register_types */ {},
            /* source_constant_arguments */ {},
            via_type_of_features_added,
            /* class_interval_context */ interval_1_4_f,
            caller_class_interval),
        (KindFrames{
            test::make_taint_config(
                test_kind_one,
                test::FrameProperties{
                    .callee_port = callee_port,
                    .callee = two,
                    .call_position = call_position,
                    .class_interval_context = interval_2_3_f,
                    .distance = 2,
                    .call_info = CallInfo::callsite()}),
            test::make_taint_config(
                test_kind_one,
                test::FrameProperties{
                    .callee_port = callee_port,
                    .callee = two,
                    .call_position = call_position,
                    .class_interval_context = interval_1_4_f,
                    .distance = 2,
                    .origins = MethodSet{two},
                    .call_info = CallInfo::callsite()}),
        }));
  }
}

TEST_F(KindFramesTest, PropagateCrtex) {
  auto context = test::make_empty_context();
  context.options = test::make_default_options();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));
  auto interval_one = ClassIntervals::Interval::finite(2, 3);
  auto interval_two = ClassIntervals::Interval::finite(4, 5);
  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");
  auto* test_kind_one = context.kind_factory->get("TestSinkOne");
  auto* call_position = context.positions->get("Test.java", 1);

  // Test propagating crtex frames (callee port == anchor).
  auto crtex_frames = KindFrames{
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Anchor)),
              .origins = MethodSet{one},
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"%programmatic_leaf_name%"})},
              .call_info = CallInfo::origin()}),
      test::make_taint_config(
          test_kind_one,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Anchor)),
              .origins = MethodSet{one},
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"constant value"})},
              .call_info = CallInfo::origin()}),
  };

  auto canonical_callee_port =
      AccessPath(Root(Root::Kind::Argument, 0)).canonicalize_for_method(two);
  auto expected_instantiated_name =
      CanonicalName(CanonicalName::InstantiatedValue{two->signature()});
  auto propagated_crtex_frames = crtex_frames.propagate_crtex_leaf_frames(
      /* callee */ two,
      canonical_callee_port,
      call_position,
      /* locally_inferred_features */ FeatureMayAlwaysSet{feature_one},
      /* maximum_source_sink_distance */ 100,
      context,
      /* source_register_types */ {},
      CallClassIntervalContext(),
      ClassIntervals::Interval::top());
  EXPECT_EQ(
      propagated_crtex_frames,
      (KindFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = canonical_callee_port,
                  .callee = two,
                  .call_position = call_position,
                  .origins = MethodSet{one},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .canonical_names =
                      CanonicalNameSetAbstractDomain{
                          expected_instantiated_name},
                  .call_info = CallInfo::callsite()}),
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = canonical_callee_port,
                  .callee = two,
                  .call_position = call_position,
                  .origins = MethodSet{one},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .canonical_names =
                      CanonicalNameSetAbstractDomain(CanonicalName(
                          CanonicalName::InstantiatedValue{"constant value"})),
                  .call_info = CallInfo::callsite()}),
      }));

  // Test propagating crtex-like frames (callee port == anchor.<path>),
  // specifically, propagate the propagated frames above again. These frames
  // originate from crtex leaves, but are not themselves the leaves.
  std::vector<const Feature*> via_type_of_features_added;
  EXPECT_EQ(
      propagated_crtex_frames.propagate(
          /* callee */ two,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          call_position,
          /* locally_inferred_featues */ FeatureMayAlwaysSet{feature_two},
          /* maximum_source_sink_distance */ 100,
          context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {},
          via_type_of_features_added,
          CallClassIntervalContext(),
          ClassIntervals::Interval::top()),
      (KindFrames{
          test::make_taint_config(
              test_kind_one,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = call_position,
                  .distance = 1,
                  .origins = MethodSet{one},
                  .inferred_features =
                      FeatureMayAlwaysSet{feature_one, feature_two},
                  .call_info = CallInfo::callsite()}),
      }));
}

} // namespace marianatrench

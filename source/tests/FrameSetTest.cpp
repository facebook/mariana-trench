/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/FrameSet.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class FrameSetTest : public test::Test {};

TEST_F(FrameSetTest, Add) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));
  auto* two = context.methods->create(
      redex::create_void_method(scope, "LOther;", "two"));

  auto* source_kind = context.kinds->get("TestSource");
  auto* field = DexString::make_string("Field");
  auto* feature_one = context.features->get("FeatureOne");
  auto* feature_two = context.features->get("FeatureTwo");
  auto* user_feature_one = context.features->get("UserFeatureOne");

  FrameSet frames;
  EXPECT_TRUE(frames.is_bottom());
  EXPECT_TRUE(frames.empty());
  EXPECT_EQ(frames.kind(), nullptr);

  frames.add(test::make_frame(
      source_kind,
      test::FrameProperties{
          .origins = MethodSet{one},
          .inferred_features = FeatureMayAlwaysSet{feature_one}}));
  EXPECT_FALSE(frames.is_bottom());
  EXPECT_EQ(frames.kind(), source_kind);
  EXPECT_EQ(
      frames,
      FrameSet{test::make_frame(
          source_kind,
          test::FrameProperties{
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one}})});

  frames.add(test::make_frame(
      source_kind,
      test::FrameProperties{
          .origins = MethodSet{two},
          .inferred_features = FeatureMayAlwaysSet{feature_two},
          .user_features = FeatureSet{user_feature_one}}));
  EXPECT_EQ(
      frames,
      FrameSet{test::make_frame(
          source_kind,
          test::FrameProperties{
              .origins = MethodSet{one, two},
              .inferred_features =
                  FeatureMayAlwaysSet::make_may({feature_one, feature_two}),
              .user_features = FeatureSet{user_feature_one}})});

  frames.add(test::make_frame(
      source_kind,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .callee = one,
          .call_position = context.positions->unknown(),
          .distance = 3,
          .origins = MethodSet{one},
          .inferred_features = FeatureMayAlwaysSet{feature_one}}));
  EXPECT_EQ(
      frames,
      (FrameSet{
          test::make_frame(
              source_kind,
              test::FrameProperties{
                  .origins = MethodSet{one, two},
                  .inferred_features =
                      FeatureMayAlwaysSet::make_may({feature_one, feature_two}),
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_frame(
              source_kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = one,
                  .call_position = context.positions->unknown(),
                  .distance = 3,
                  .origins = MethodSet{one},
                  .inferred_features = FeatureMayAlwaysSet{feature_one}}),
      }));

  frames.add(test::make_frame(
      source_kind,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .callee = one,
          .call_position = context.positions->unknown(),
          .distance = 3,
          .origins = MethodSet{two},
          .inferred_features = FeatureMayAlwaysSet{feature_one, feature_two}}));
  EXPECT_EQ(
      frames,
      (FrameSet{
          test::make_frame(
              source_kind,
              test::FrameProperties{
                  .origins = MethodSet{one, two},
                  .inferred_features =
                      FeatureMayAlwaysSet::make_may({feature_one, feature_two}),
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_frame(
              source_kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = one,
                  .call_position = context.positions->unknown(),
                  .distance = 3,
                  .origins = MethodSet{one, two},
                  .inferred_features = FeatureMayAlwaysSet(
                      /* may */ FeatureSet{feature_one, feature_two},
                      /* always */ FeatureSet{feature_one})}),
      }));

  // Frames with different callee ports are not merged.
  frames.add(test::make_frame(
      source_kind,
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return), Path{field}),
          .callee = one,
          .call_position = context.positions->unknown(),
          .distance = 3,
          .origins = MethodSet{one},
          .inferred_features = FeatureMayAlwaysSet{feature_one}}));
  EXPECT_EQ(
      frames,
      (FrameSet{
          test::make_frame(
              source_kind,
              test::FrameProperties{
                  .origins = MethodSet{one, two},
                  .inferred_features =
                      FeatureMayAlwaysSet::make_may({feature_one, feature_two}),
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_frame(
              source_kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = one,
                  .call_position = context.positions->unknown(),
                  .distance = 3,
                  .origins = MethodSet{one, two},
                  .inferred_features = FeatureMayAlwaysSet(
                      /* may */ FeatureSet{feature_one, feature_two},
                      /* always */ FeatureSet{feature_one}),
              }),
          test::make_frame(
              source_kind,
              test::FrameProperties{
                  .callee_port =
                      AccessPath(Root(Root::Kind::Return), Path{field}),
                  .callee = one,
                  .call_position = context.positions->unknown(),
                  .distance = 3,
                  .origins = MethodSet{one},
                  .inferred_features = FeatureMayAlwaysSet{feature_one}}),
      }));
}

TEST_F(FrameSetTest, ArtificialSources) {
  auto context = test::make_empty_context();

  auto* field_one = DexString::make_string("FieldOne");
  auto* field_two = DexString::make_string("FieldTwo");
  auto* field_three = DexString::make_string("FieldThree");
  auto* feature_one = context.features->get("FeatureOne");
  auto* feature_two = context.features->get("FeatureTwo");
  auto* user_feature_one = context.features->get("UserFeatureOne");
  auto* user_feature_two = context.features->get("UserFeatureTwo");

  FrameSet frames;
  frames.add(test::make_frame(
      Kinds::artificial_source(),
      test::FrameProperties{
          .callee_port = AccessPath(
              Root(Root::Kind::Argument, 0), Path{field_one, field_two}),
          .inferred_features = FeatureMayAlwaysSet{feature_one},
          .user_features = FeatureSet{user_feature_one}}));
  EXPECT_EQ(
      frames,
      FrameSet{test::make_frame(
          Kinds::artificial_source(),
          test::FrameProperties{
              .callee_port = AccessPath(
                  Root(Root::Kind::Argument, 0), Path{field_one, field_two}),
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one}})});

  // We use the common prefix of the callee port.
  frames.add(test::make_frame(
      Kinds::artificial_source(),
      test::FrameProperties{
          .callee_port = AccessPath(
              Root(Root::Kind::Argument, 0), Path{field_one, field_three}),
          .inferred_features = FeatureMayAlwaysSet{feature_two},
          .user_features = FeatureSet{user_feature_two}}));
  EXPECT_EQ(
      frames,
      FrameSet{test::make_frame(
          Kinds::artificial_source(),
          test::FrameProperties{
              .callee_port =
                  AccessPath(Root(Root::Kind::Argument, 0), Path{field_one}),
              .inferred_features =
                  FeatureMayAlwaysSet::make_may({feature_one, feature_two}),
              .user_features =
                  FeatureSet{user_feature_one, user_feature_two}})});

  // We do not merge when the port root is different.
  frames.add(test::make_frame(
      Kinds::artificial_source(),
      test::FrameProperties{
          .callee_port =
              AccessPath(Root(Root::Kind::Argument, 1), Path{field_three})}));
  EXPECT_EQ(
      frames,
      (FrameSet{
          test::make_frame(
              Kinds::artificial_source(),
              test::FrameProperties{
                  .callee_port = AccessPath(
                      Root(Root::Kind::Argument, 0), Path{field_one}),
                  .inferred_features =
                      FeatureMayAlwaysSet::make_may({feature_one, feature_two}),
                  .user_features =
                      FeatureSet{user_feature_one, user_feature_two}}),
          test::make_frame(
              Kinds::artificial_source(),
              test::FrameProperties{
                  .callee_port = AccessPath(
                      Root(Root::Kind::Argument, 1), Path{field_three})}),
      }));
}

TEST_F(FrameSetTest, Leq) {
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

  auto* test_kind = context.kinds->get("TestSink");
  auto* test_position = context.positions->get(std::nullopt, 1);

  EXPECT_TRUE(
      (FrameSet{
           test::make_frame(
               test_kind,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                   .callee = one,
                   .call_position = test_position,
                   .distance = 1,
                   .origins = MethodSet{one}}),
           test::make_frame(
               test_kind,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                   .callee = two,
                   .call_position = test_position,
                   .distance = 1,
                   .origins = MethodSet{two}}),
       })
          .leq(FrameSet{
              test::make_frame(
                  test_kind,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = one,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{one}}),
              test::make_frame(
                  test_kind,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = two,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{two}}),
              test::make_frame(
                  test_kind,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = three,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{three}}),
          }));
  EXPECT_FALSE(
      (FrameSet{
           test::make_frame(
               test_kind,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                   .callee = one,
                   .call_position = test_position,
                   .distance = 1,
                   .origins = MethodSet{one}}),
           test::make_frame(
               test_kind,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                   .callee = two,
                   .call_position = test_position,
                   .distance = 1,
                   .origins = MethodSet{two}}),
           test::make_frame(
               test_kind,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                   .callee = three,
                   .call_position = test_position,
                   .distance = 1,
                   .origins = MethodSet{three}}),
       })
          .leq(FrameSet{
              test::make_frame(
                  test_kind,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = one,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{one}}),
              test::make_frame(
                  test_kind,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = two,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{two}}),
          }));
  EXPECT_FALSE(
      (FrameSet{
           test::make_frame(
               test_kind,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                   .callee = one,
                   .call_position = test_position,
                   .distance = 1,
                   .origins = MethodSet{one}}),
           test::make_frame(
               test_kind,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                   .callee = two,
                   .call_position = test_position,
                   .distance = 1,
                   .origins = MethodSet{two}}),
       })
          .leq(FrameSet{
              test::make_frame(
                  test_kind,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = one,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{one}}),
              test::make_frame(
                  test_kind,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = two,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{two}}),
              test::make_frame(
                  test_kind,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = three,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{three}}),
          }));
  EXPECT_TRUE(
      (FrameSet{
           test::make_frame(
               test_kind,
               test::FrameProperties{
                   .callee_port =
                       AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
                   .callee = one,
                   .call_position = test_position,
                   .distance = 1,
                   .origins = MethodSet{one}}),
           test::make_frame(
               test_kind,
               test::FrameProperties{
                   .callee_port =
                       AccessPath(Root(Root::Kind::Argument, 0), Path{y}),
                   .callee = two,
                   .call_position = test_position,
                   .distance = 1,
                   .origins = MethodSet{two}}),
       })
          .leq(FrameSet{
              test::make_frame(
                  test_kind,
                  test::FrameProperties{
                      .callee_port =
                          AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
                      .callee = one,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{one}}),
              test::make_frame(
                  test_kind,
                  test::FrameProperties{
                      .callee_port =
                          AccessPath(Root(Root::Kind::Argument, 0), Path{y}),
                      .callee = two,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{two}}),
              test::make_frame(
                  test_kind,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                      .callee = three,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{three}}),
          }));
  EXPECT_TRUE(
      (FrameSet{
           test::make_frame(
               test_kind,
               test::FrameProperties{
                   .callee_port =
                       AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}),
                   .callee = one,
                   .call_position = test_position,
                   .distance = 1,
                   .origins = MethodSet{one}}),
           test::make_frame(
               test_kind,
               test::FrameProperties{
                   .callee_port =
                       AccessPath(Root(Root::Kind::Argument, 0), Path{y, x}),
                   .callee = two,
                   .call_position = test_position,
                   .distance = 1,
                   .origins = MethodSet{two}}),
           test::make_frame(
               test_kind,
               test::FrameProperties{
                   .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                   .callee = three,
                   .call_position = test_position,
                   .distance = 1,
                   .origins = MethodSet{three}}),
       })
          .leq(FrameSet{
              test::make_frame(
                  test_kind,
                  test::FrameProperties{
                      .callee_port =
                          AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}),
                      .callee = one,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{one}}),
              test::make_frame(
                  test_kind,
                  test::FrameProperties{
                      .callee_port =
                          AccessPath(Root(Root::Kind::Argument, 0), Path{y, x}),
                      .callee = two,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{two}}),
              test::make_frame(
                  test_kind,
                  test::FrameProperties{
                      .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                      .callee = three,
                      .call_position = test_position,
                      .distance = 1,
                      .origins = MethodSet{three}}),
          }));
}

TEST_F(FrameSetTest, Difference) {
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

  auto* test_kind = context.kinds->get("TestSink");
  auto* test_position = context.positions->get(std::nullopt, 1);
  auto* feature_one = context.features->get("FeatureOne");
  auto* feature_two = context.features->get("FeatureTwo");
  auto* feature_three = context.features->get("FeatureThree");
  auto* user_feature_one = context.features->get("UserFeatureOne");
  auto* user_feature_two = context.features->get("UserFeatureTwo");
  auto* user_feature_three = context.features->get("UserFeatureThree");

  FrameSet frames, initial_frames;

  // Tests with empty left hand side.
  frames.difference_with(FrameSet{});
  EXPECT_TRUE(frames.is_bottom());

  frames.difference_with(FrameSet{
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
  });
  EXPECT_TRUE(frames.is_bottom());

  initial_frames = FrameSet{
      test::make_frame(
          test_kind,
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
  frames.difference_with(FrameSet{});
  EXPECT_EQ(frames, initial_frames);

  frames = initial_frames;
  frames.difference_with(FrameSet{
      test::make_frame(
          test_kind,
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
  frames.difference_with(FrameSet{
      test::make_frame(
          test_kind,
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
  frames.difference_with(FrameSet{
      test::make_frame(
          test_kind,
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
  frames.difference_with(FrameSet{
      test::make_frame(
          test_kind,
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
  frames.difference_with(FrameSet{
      test::make_frame(
          test_kind,
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

  frames = FrameSet{
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two},
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_two}}),
  };
  frames.difference_with(FrameSet{
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two},
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_two}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three},
              .inferred_features = FeatureMayAlwaysSet{feature_three},
              .user_features = FeatureSet{user_feature_three}}),
  });
  EXPECT_TRUE(frames.is_bottom());

  frames = FrameSet{
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three}}),
  };
  frames.difference_with(FrameSet{
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
  });
  EXPECT_EQ(
      frames,
      (FrameSet{
          test::make_frame(
              test_kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = three,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{three}}),
      }));

  frames = FrameSet{
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
  };
  frames.difference_with(FrameSet{
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three}}),
  });
  EXPECT_EQ(
      frames,
      (FrameSet{
          test::make_frame(
              test_kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .callee = one,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{one}}),
      }));

  frames = FrameSet{
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0), Path{y}),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
  };
  frames.difference_with(FrameSet{
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0), Path{y}),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three}}),
  });
  EXPECT_TRUE(frames.is_bottom());

  frames = FrameSet{
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one, two}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one, three}}),
  };
  frames.difference_with(FrameSet{
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one, two, three}}),
  });
  EXPECT_EQ(
      frames,
      (FrameSet{
          test::make_frame(
              test_kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = one,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{one, two}}),
          test::make_frame(
              test_kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{two}}),
      }));
}

TEST_F(FrameSetTest, Map) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));
  auto* three = context.methods->create(
      redex::create_void_method(scope, "LThree;", "three"));

  auto* test_kind = context.kinds->get("TestSink");
  auto* test_position = context.positions->get(std::nullopt, 1);
  auto* feature_one = context.features->get("FeatureOne");

  auto frames = FrameSet{
      test::make_frame(
          test_kind, test::FrameProperties{.origins = MethodSet{one}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three}}),
  };
  frames.map([feature_one](Frame& frame) {
    frame.add_inferred_features(FeatureMayAlwaysSet{feature_one});
  });
  EXPECT_EQ(
      frames,
      (FrameSet{
          test::make_frame(
              test_kind,
              test::FrameProperties{
                  .origins = MethodSet{one},
                  .locally_inferred_features =
                      FeatureMayAlwaysSet{feature_one}}),
          test::make_frame(
              test_kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{two},
                  .locally_inferred_features =
                      FeatureMayAlwaysSet{feature_one}}),
          test::make_frame(
              test_kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = three,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{three},
                  .locally_inferred_features =
                      FeatureMayAlwaysSet{feature_one}}),
      }));
}

TEST_F(FrameSetTest, Filter) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));
  auto* three = context.methods->create(
      redex::create_void_method(scope, "LThree;", "three"));

  auto* test_kind = context.kinds->get("TestSink");
  auto* test_position = context.positions->get(std::nullopt, 1);

  auto frames = FrameSet{
      test::make_frame(
          test_kind, test::FrameProperties{.origins = MethodSet{one}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three}}),
  };
  frames.filter(
      [](const Frame& frame) { return frame.callee_port().root().is_leaf(); });
  EXPECT_EQ(
      frames,
      (FrameSet{
          test::make_frame(
              test_kind, test::FrameProperties{.origins = MethodSet{one}}),
      }));
}

TEST_F(FrameSetTest, Propagate) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto* test_kind = context.kinds->get("TestSink");
  auto* call_position = context.positions->get("Test.java", 1);

  auto frames = FrameSet{
      test::make_frame(
          test_kind,
          test::FrameProperties{.callee = two, .origins = MethodSet{two}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_frame(
          test_kind,
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
          /* caller */ one,
          /* callee */ two,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          call_position,
          /* maximum_source_sink_distance */ 100,
          context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {}),
      (FrameSet{
          test::make_frame(
              test_kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = call_position,
                  .distance = 1,
                  .origins = MethodSet{one, two},
                  .locally_inferred_features = FeatureMayAlwaysSet::bottom()}),
          test::make_frame(
              test_kind,
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

TEST_F(FrameSetTest, WithKind) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto* test_kind = context.kinds->get("TestSink");
  auto* test_position = context.positions->get(std::nullopt, 1);

  auto frames = FrameSet{
      test::make_frame(
          test_kind, test::FrameProperties{.origins = MethodSet{one}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}})};

  auto new_kind = context.kinds->get("TestSink2");
  auto frame_with_new_kind = frames.with_kind(new_kind);

  EXPECT_EQ(frame_with_new_kind.kind(), new_kind);
  EXPECT_EQ(
      frame_with_new_kind,
      (FrameSet{
          test::make_frame(
              new_kind, test::FrameProperties{.origins = MethodSet{one}}),
          test::make_frame(
              new_kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{two}})}));
}

TEST_F(FrameSetTest, PartitionMap) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));
  auto* test_kind = context.kinds->get("TestSink");

  auto frames = FrameSet{
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = one,
              .origins = MethodSet{one}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .distance = 1,
              .origins = MethodSet{two}}),
      test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Anchor)),
              .origins = MethodSet{one},
              .canonical_names = CanonicalNameSetAbstractDomain{CanonicalName(
                  CanonicalName::TemplateValue{"%programmatic_leaf_name%"})}}),
  };

  auto partitions = frames.partition_map<bool>(
      [](const Frame& frame) { return frame.is_crtex_producer_declaration(); });

  FrameSet crtex_partition;
  for (auto frame : partitions[true]) {
    crtex_partition.add(frame);
  }
  EXPECT_EQ(
      crtex_partition,
      FrameSet{test::make_frame(
          test_kind,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Anchor)),
              .origins = MethodSet{one},
              .canonical_names = CanonicalNameSetAbstractDomain{
                  CanonicalName(CanonicalName::TemplateValue{
                      "%programmatic_leaf_name%"})}})});

  FrameSet non_crtex_partition;
  for (auto frame : partitions[false]) {
    non_crtex_partition.add(frame);
  }
  EXPECT_EQ(
      non_crtex_partition,
      (FrameSet{
          test::make_frame(
              test_kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = one,
                  .origins = MethodSet{one}}),
          test::make_frame(
              test_kind,
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .distance = 1,
                  .origins = MethodSet{two}}),
      }));
}

} // namespace marianatrench

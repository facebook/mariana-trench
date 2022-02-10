/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/FeatureMayAlwaysSet.h>
#include <mariana-trench/LocalPositionSet.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/Taint.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class TaintTest : public test::Test {};

TEST_F(TaintTest, Insertion) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one = context.methods->create(
      redex::create_void_method(scope, "LClass;", "one"));
  auto* two = context.methods->create(
      redex::create_void_method(scope, "LOther;", "two"));

  Taint taint;
  EXPECT_EQ(taint, Taint());

  taint.add(test::make_frame(
      /* kind */ context.kinds->get("TestSource"), test::FrameProperties{}));
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{}),
      }));

  taint.add(test::make_frame(
      /* kind */ context.kinds->get("OtherSource"), test::FrameProperties{}));
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{}),
          test::make_frame(
              /* kind */ context.kinds->get("OtherSource"),
              test::FrameProperties{}),
      }));

  taint.add(test::make_frame(
      /* kind */ context.kinds->get("IndirectSource"),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .callee = one,
          .call_position = context.positions->unknown(),
          .distance = 2,
          .origins = MethodSet{one}}));
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{}),
          test::make_frame(
              /* kind */ context.kinds->get("OtherSource"),
              test::FrameProperties{}),
          test::make_frame(
              /* kind */ context.kinds->get("IndirectSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = one,
                  .call_position = context.positions->unknown(),
                  .distance = 2,
                  .origins = MethodSet{one}}),
      }));

  taint.add(test::make_frame(
      /* kind */ context.kinds->get("IndirectSource"),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .callee = one,
          .call_position = context.positions->unknown(),
          .distance = 3,
          .origins = MethodSet{two}}));
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{}),
          test::make_frame(
              /* kind */ context.kinds->get("OtherSource"),
              test::FrameProperties{}),
          test::make_frame(
              /* kind */ context.kinds->get("IndirectSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = one,
                  .call_position = context.positions->unknown(),
                  .distance = 2,
                  .origins = MethodSet{one, two}}),
      }));

  taint.add(test::make_frame(
      /* kind */ context.kinds->get("IndirectSource"),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .callee = two,
          .call_position = context.positions->unknown(),
          .distance = 3,
          .origins = MethodSet{two}}));
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{}),
          test::make_frame(
              /* kind */ context.kinds->get("OtherSource"),
              test::FrameProperties{}),
          test::make_frame(
              /* kind */ context.kinds->get("IndirectSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = one,
                  .call_position = context.positions->unknown(),
                  .distance = 2,
                  .origins = MethodSet{one, two}}),
          test::make_frame(
              /* kind */ context.kinds->get("IndirectSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = two,
                  .call_position = context.positions->unknown(),
                  .distance = 3,
                  .origins = MethodSet{two}}),
      }));
}

TEST_F(TaintTest, Difference) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));
  auto* three = context.methods->create(
      redex::create_void_method(scope, "LThree;", "three"));

  auto* test_position = context.positions->get(std::nullopt, 1);
  auto* feature_one = context.features->get("FeatureOne");
  auto* feature_two = context.features->get("FeatureTwo");
  auto* feature_three = context.features->get("FeatureThree");
  auto* user_feature_one = context.features->get("UserFeatureOne");
  auto* user_feature_two = context.features->get("UserFeatureTwo");
  auto* user_feature_three = context.features->get("UserFeatureThree");

  Taint taint = Taint{
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one}}),
      test::make_frame(
          /* kind */ context.kinds->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two},
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_two}}),
  };
  taint.difference_with(Taint{
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one}}),
      test::make_frame(
          /* kind */ context.kinds->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two},
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_two}}),
      test::make_frame(
          /* kind */ context.kinds->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three},
              .inferred_features = FeatureMayAlwaysSet{feature_three},
              .user_features = FeatureSet{user_feature_three}}),
  });
  EXPECT_TRUE(taint.is_bottom());

  taint = Taint{
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 2,
              .origins = MethodSet{one}}),
      test::make_frame(
          /* kind */ context.kinds->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three}}),
  };
  taint.difference_with(Taint{
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
  });
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_frame(
              /* kind */ context.kinds->get("OtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{two}}),
          test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = three,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{three}}),
      }));

  taint = Taint{
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_frame(
          /* kind */ context.kinds->get("SomeOtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
  };
  taint.difference_with(Taint{
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one}}),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two}}),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three}}),
  });
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_frame(
              /* kind */ context.kinds->get("SomeOtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{two}}),
      }));
}

TEST_F(TaintTest, AddFeatures) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));
  auto* three = context.methods->create(
      redex::create_void_method(scope, "LThree;", "three"));

  auto* test_position = context.positions->get(std::nullopt, 1);
  auto* feature_one = context.features->get("FeatureOne");
  auto* feature_two = context.features->get("FeatureTwo");
  auto* feature_three = context.features->get("FeatureThree");
  auto* user_feature_one = context.features->get("UserFeatureOne");
  auto* user_feature_two = context.features->get("UserFeatureTwo");

  auto taint = Taint{
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one}}),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two},
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_two}}),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three}}),
  };
  taint.add_inferred_features(
      FeatureMayAlwaysSet::make_always({feature_three}));
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .callee = one,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{one},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .locally_inferred_features =
                      FeatureMayAlwaysSet{feature_three},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{two},
                  .inferred_features = FeatureMayAlwaysSet{feature_two},
                  .locally_inferred_features =
                      FeatureMayAlwaysSet{feature_three},
                  .user_features = FeatureSet{user_feature_two}}),
          test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = three,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{three},
                  .locally_inferred_features =
                      FeatureMayAlwaysSet{feature_three}}),
      }));
}

TEST_F(TaintTest, Propagate) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));
  auto* three = context.methods->create(
      redex::create_void_method(scope, "LThree;", "three"));
  auto* four = context.methods->create(
      redex::create_void_method(scope, "LFour;", "four"));

  auto* test_position = context.positions->get(std::nullopt, 1);
  auto* feature_one = context.features->get("FeatureOne");
  auto* feature_two = context.features->get("FeatureTwo");
  auto* feature_three = context.features->get("FeatureThree");
  auto* user_feature_one = context.features->get("UserFeatureOne");
  auto* user_feature_two = context.features->get("UserFeatureTwo");

  auto taint = Taint{
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .origins = MethodSet{one},
              .user_features = FeatureSet{user_feature_one}}),
      test::make_frame(
          /* kind */ context.kinds->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = two,
              .call_position = test_position,
              .distance = 2,
              .origins = MethodSet{two},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one}}),
      test::make_frame(
          /* kind */ context.kinds->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .locally_inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_one, user_feature_two}}),
  };

  // When propagating, all user features become inferred features.
  EXPECT_EQ(
      taint.propagate(
          /* caller */ one,
          /* callee */ four,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 2)),
          /* call_position */ context.positions->get("Test.java", 1),
          /* maximum_source_sink_distance */ 100,
          /* extra_features */ FeatureMayAlwaysSet{feature_three},
          /* context */ context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {}),
      (Taint{
          test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 2)),
                  .callee = four,
                  .call_position = context.positions->get("Test.java", 1),
                  .distance = 1,
                  .origins = MethodSet{one},
                  .inferred_features = FeatureMayAlwaysSet{user_feature_one},
                  .locally_inferred_features =
                      FeatureMayAlwaysSet{feature_three}}),
          test::make_frame(
              /* kind */ context.kinds->get("OtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 2)),
                  .callee = four,
                  .call_position = context.positions->get("Test.java", 1),
                  .distance = 2,
                  .origins = MethodSet{two, three},
                  .inferred_features = FeatureMayAlwaysSet(
                      /* may */ FeatureSet{user_feature_two, feature_two},
                      /* always */
                      FeatureSet{user_feature_one, feature_one}),
                  .locally_inferred_features =
                      FeatureMayAlwaysSet{feature_three}}),
      }));
}

TEST_F(TaintTest, TransformKind) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));
  auto* three = context.methods->create(
      redex::create_void_method(scope, "LThree;", "three"));

  auto* test_position = context.positions->get(std::nullopt, 1);
  auto* feature_one = context.features->get("FeatureOne");
  auto* feature_two = context.features->get("FeatureTwo");
  auto* user_feature_one = context.features->get("UserFeatureOne");
  auto* user_feature_two = context.features->get("UserFeatureTwo");

  auto* test_source = context.kinds->get("TestSource");
  auto* transformed_test_source = context.kinds->get("TransformedTestSource");
  auto* transformed_test_source2 = context.kinds->get("TransformedTestSource2");

  auto taint = Taint{
      test::make_frame(
          /* kind */ test_source,
          test::FrameProperties{
              .origins = MethodSet{one},
              .user_features = FeatureSet{user_feature_one}}),
      test::make_frame(
          /* kind */ context.kinds->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = two,
              .call_position = test_position,
              .distance = 2,
              .origins = MethodSet{two},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one}}),
      test::make_frame(
          /* kind */ context.kinds->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three},
              .inferred_features =
                  FeatureMayAlwaysSet{feature_one, feature_two},
              .user_features = FeatureSet{user_feature_one, user_feature_two}}),
  };

  // This works the same way as filter.
  auto empty_taint = taint.transform_map_kind(
      [](const auto* /* unused kind */) { return std::vector<const Kind*>(); },
      [](FrameSet&) {});
  EXPECT_EQ(empty_taint, Taint::bottom());

  // This actually performs a transformation.
  auto map_test_source_taint = taint.transform_map_kind(
      [test_source,
       transformed_test_source](const auto* kind) -> std::vector<const Kind*> {
        if (kind == test_source) {
          return {transformed_test_source};
        }
        return {kind};
      },
      [](FrameSet&) {});
  EXPECT_EQ(
      map_test_source_taint,
      (Taint{
          test::make_frame(
              /* kind */ transformed_test_source,
              test::FrameProperties{
                  .origins = MethodSet{one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_frame(
              /* kind */ context.kinds->get("OtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .callee = two,
                  .call_position = test_position,
                  .distance = 2,
                  .origins = MethodSet{two},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_frame(
              /* kind */ context.kinds->get("OtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = three,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{three},
                  .inferred_features =
                      FeatureMayAlwaysSet{feature_one, feature_two},
                  .user_features =
                      FeatureSet{user_feature_one, user_feature_two}}),
      }));

  // Another transformation. Covers mapping transformed frames.
  map_test_source_taint = taint.transform_map_kind(
      [test_source, transformed_test_source](const auto* kind) {
        if (kind == test_source) {
          return std::vector<const Kind*>{transformed_test_source};
        }
        return std::vector<const Kind*>{kind};
      },
      [feature_one](FrameSet& frames) {
        frames.add_inferred_features(FeatureMayAlwaysSet{feature_one});
      });
  EXPECT_EQ(
      map_test_source_taint,
      (Taint{
          test::make_frame(
              /* kind */ transformed_test_source,
              test::FrameProperties{
                  .origins = MethodSet{one},
                  .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_frame(
              /* kind */ context.kinds->get("OtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .callee = two,
                  .call_position = test_position,
                  .distance = 2,
                  .origins = MethodSet{two},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_frame(
              /* kind */ context.kinds->get("OtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = three,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{three},
                  .inferred_features =
                      FeatureMayAlwaysSet{feature_one, feature_two},
                  .user_features =
                      FeatureSet{user_feature_one, user_feature_two}}),
      }));

  // Tests one -> many transformations (with features).
  map_test_source_taint = taint.transform_map_kind(
      [test_source, transformed_test_source, transformed_test_source2](
          const auto* kind) {
        if (kind == test_source) {
          return std::vector<const Kind*>{
              test_source, transformed_test_source, transformed_test_source2};
        }
        return std::vector<const Kind*>{};
      },
      [feature_one](FrameSet& frames) {
        frames.add_inferred_features(FeatureMayAlwaysSet{feature_one});
      });
  EXPECT_EQ(
      map_test_source_taint,
      (Taint{
          test::make_frame(
              /* kind */ test_source,
              test::FrameProperties{
                  .origins = MethodSet{one},
                  .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_frame(
              /* kind */ transformed_test_source,
              test::FrameProperties{
                  .origins = MethodSet{one},
                  .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_frame(
              /* kind */ transformed_test_source2,
              test::FrameProperties{
                  .origins = MethodSet{one},
                  .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
      }));
}

TEST_F(TaintTest, AppendCalleePort) {
  auto context = test::make_empty_context();

  const auto* path_element1 = DexString::make_string("field1");
  const auto* path_element2 = DexString::make_string("field2");

  auto taint = Taint{
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"), test::FrameProperties{}),
      test::make_frame(
          Kinds::artificial_source(),
          test::FrameProperties{
              .callee_port = AccessPath(
                  Root(Root::Kind::Argument), Path{path_element1})})};

  taint.append_callee_port(path_element2, [](const Kind* kind) {
    return kind == Kinds::artificial_source();
  });

  EXPECT_EQ(
      taint,
      (Taint{
          test::make_frame(
              /* kind */ context.kinds->get("TestSource"),
              test::FrameProperties{}),
          test::make_frame(
              Kinds::artificial_source(),
              test::FrameProperties{
                  .callee_port = AccessPath(
                      Root(Root::Kind::Argument),
                      Path{path_element1, path_element2})})}));
}

TEST_F(TaintTest, UpdateNonLeafPositions) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method1 =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* method2 =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));
  auto* method3 = context.methods->create(
      redex::create_void_method(scope, "LThree;", "three"));

  auto dex_position1 = DexPosition(/* line */ 1);
  auto dex_position2 = DexPosition(/* line */ 2);
  auto dex_position3 = DexPosition(/* line */ 3);

  auto position1 = context.positions->get(method1, &dex_position1);
  auto position2 = context.positions->get(method2, &dex_position2);
  auto position3 = context.positions->get(method2, &dex_position3);

  auto taint = Taint{
      test::make_frame(
          /* kind */ context.kinds->get("LeafFrame"), test::FrameProperties{}),
      test::make_frame(
          /* kind */ context.kinds->get("NonLeafFrame1"),
          test::FrameProperties{
              .callee = method1,
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .call_position = position1}),
      test::make_frame(
          /* kind */ context.kinds->get("NonLeafFrame2"),
          test::FrameProperties{
              .callee = method2,
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .call_position = position2}),
      test::make_frame(
          /* kind */ context.kinds->get("NonLeafFrame3"),
          test::FrameProperties{
              .callee = method3,
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .call_position = position3})};

  taint.update_non_leaf_positions(
      [&](const Method* callee,
          const AccessPath& callee_port,
          const Position* position) {
        if (callee == method1) {
          return context.positions->get(
              position, /* line */ 10, /* start */ 11, /* end */ 12);
        } else if (callee_port == AccessPath(Root(Root::Kind::Argument))) {
          return context.positions->get(
              position, /* line */ 20, /* start */ 21, /* end */ 22);
        }
        return position;
      },
      [&](const LocalPositionSet& local_positions) {
        LocalPositionSet new_local_positions = local_positions;
        new_local_positions.add(position1);
        return new_local_positions;
      });

  LocalPositionSet expected_local_positions;
  expected_local_positions.add(position1);

  EXPECT_EQ(
      taint,
      (Taint{
          test::make_frame(
              /* kind */ context.kinds->get("LeafFrame"),
              test::FrameProperties{}),
          test::make_frame(
              /* kind */ context.kinds->get("NonLeafFrame1"),
              test::FrameProperties{
                  .callee = method1,
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .call_position = context.positions->get(
                      position1, /* line */ 10, /* start */ 11, /* end */ 12),
                  .local_positions = expected_local_positions}),
          test::make_frame(
              /* kind */ context.kinds->get("NonLeafFrame2"),
              test::FrameProperties{
                  .callee = method2,
                  .callee_port = AccessPath(Root(Root::Kind::Argument)),
                  .call_position = context.positions->get(
                      position2, /* line */ 20, /* start */ 21, /* end */ 22),
                  .local_positions = expected_local_positions}),
          test::make_frame(
              /* kind */ context.kinds->get("NonLeafFrame3"),
              test::FrameProperties{
                  .callee = method3,
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .call_position = position3,
                  .local_positions = expected_local_positions})}));
}

TEST_F(TaintTest, FilterInvalidFrames) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method1 =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));

  // Filter by callee
  auto taint = Taint{
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"), test::FrameProperties{}),
      test::make_frame(
          Kinds::artificial_source(),
          test::FrameProperties{
              .callee = method1,
              .callee_port = AccessPath(Root(Root::Kind::Argument))})};
  taint.filter_invalid_frames(
      /* is_valid */
      [&](const Method* MT_NULLABLE callee,
          const AccessPath& /* callee_port */,
          const Kind* /* kind */) { return callee == nullptr; });
  EXPECT_EQ(
      taint,
      (Taint{test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{})}));

  // Filter by callee port
  taint = Taint{
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"), test::FrameProperties{}),
      test::make_frame(
          Kinds::artificial_source(),
          test::FrameProperties{
              .callee = method1,
              .callee_port = AccessPath(Root(Root::Kind::Argument))})};
  taint.filter_invalid_frames(
      /* is_valid */
      [&](const Method* MT_NULLABLE /* callee */,
          const AccessPath& callee_port,
          const Kind* /* kind */) {
        return callee_port == AccessPath(Root(Root::Kind::Argument));
      });
  EXPECT_EQ(
      taint,
      (Taint{test::make_frame(
          Kinds::artificial_source(),
          test::FrameProperties{
              .callee = method1,
              .callee_port = AccessPath(Root(Root::Kind::Argument))})}));

  // Filter by kind
  taint = Taint{
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"), test::FrameProperties{}),
      test::make_frame(
          Kinds::artificial_source(),
          test::FrameProperties{
              .callee = method1,
              .callee_port = AccessPath(Root(Root::Kind::Argument))})};
  taint.filter_invalid_frames(
      /* is_valid */
      [&](const Method* MT_NULLABLE /* callee */,
          const AccessPath& /* callee_port */,
          const Kind* kind) { return kind != Kinds::artificial_source(); });
  EXPECT_EQ(
      taint,
      (Taint{test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{})}));
}

TEST_F(TaintTest, ContainsKind) {
  auto context = test::make_empty_context();

  auto taint = Taint{
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"), test::FrameProperties{}),
      test::make_frame(Kinds::artificial_source(), test::FrameProperties{})};

  EXPECT_TRUE(taint.contains_kind(Kinds::artificial_source()));
  EXPECT_TRUE(taint.contains_kind(context.kinds->get("TestSource")));
  EXPECT_FALSE(taint.contains_kind(context.kinds->get("TestSink")));
}

TEST_F(TaintTest, PartitionByKind) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method1 =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* method2 =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto taint = Taint{
      test::make_frame(
          /* kind */ context.kinds->get("TestSource1"),
          test::FrameProperties{}),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource2"),
          test::FrameProperties{}),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource3"),
          test::FrameProperties{.callee = method1}),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource3"),
          test::FrameProperties{.callee = method2})};

  auto taint_by_kind = taint.partition_by_kind();
  EXPECT_TRUE(taint_by_kind.size() == 3);
  EXPECT_EQ(
      taint_by_kind[context.kinds->get("TestSource1")],
      Taint{test::make_frame(
          /* kind */ context.kinds->get("TestSource1"),
          test::FrameProperties{})});
  EXPECT_EQ(
      taint_by_kind[context.kinds->get("TestSource2")],
      Taint{test::make_frame(
          /* kind */ context.kinds->get("TestSource2"),
          test::FrameProperties{})});
  EXPECT_EQ(
      taint_by_kind[context.kinds->get("TestSource3")],
      (Taint{
          test::make_frame(
              /* kind */ context.kinds->get("TestSource3"),
              test::FrameProperties{.callee = method1}),
          test::make_frame(
              /* kind */ context.kinds->get("TestSource3"),
              test::FrameProperties{.callee = method2})}));
}

TEST_F(TaintTest, FeaturesJoined) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method1 =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* method2 =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto* feature1 = context.features->get("Feature1");
  auto* feature2 = context.features->get("Feature2");
  auto* feature3 = context.features->get("Feature3");

  auto taint = Taint{
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee = method1,
              .inferred_features = FeatureMayAlwaysSet{feature1}}),
      test::make_frame(
          /* kind */ context.kinds->get("TestSource"),
          test::FrameProperties{
              .callee = method2,
              .inferred_features = FeatureMayAlwaysSet(
                  /* may */ FeatureSet{feature2},
                  /* always */ FeatureSet{feature3}),
              .locally_inferred_features = FeatureMayAlwaysSet{feature1}})};

  // In practice, features_joined() is called on `Taint` objects with only one
  // underlying kind. The expected behavior is to first merge locally inferred
  // features within each frame (this is an add() operation, not join()), then
  // perform a join() across all frames that have different callees/positions.

  EXPECT_EQ(
      taint.features_joined(),
      FeatureMayAlwaysSet(
          /* may */ FeatureSet{feature2, feature3},
          /* always */ FeatureSet{feature1}));
}

} // namespace marianatrench

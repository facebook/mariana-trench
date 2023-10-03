/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <unordered_set>

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

  taint.add(test::make_taint_config(
      /* kind */ context.kind_factory->get("TestSource"),
      test::FrameProperties{}));
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{}),
      }));

  taint.add(test::make_taint_config(
      /* kind */ context.kind_factory->get("OtherSource"),
      test::FrameProperties{}));
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{}),
      }));

  taint.add(test::make_taint_config(
      /* kind */ context.kind_factory->get("IndirectSource"),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .callee = one,
          .call_position = context.positions->unknown(),
          .distance = 2,
          .origins = MethodSet{one},
          .call_kind = CallKind::callsite(),
      }));
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("IndirectSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = one,
                  .call_position = context.positions->unknown(),
                  .distance = 2,
                  .origins = MethodSet{one},
                  .call_kind = CallKind::callsite(),
              }),
      }));

  taint.add(test::make_taint_config(
      /* kind */ context.kind_factory->get("IndirectSource"),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .callee = one,
          .call_position = context.positions->unknown(),
          .distance = 3,
          .origins = MethodSet{two},
          .call_kind = CallKind::callsite(),
      }));
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("IndirectSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = one,
                  .call_position = context.positions->unknown(),
                  .distance = 2,
                  .origins = MethodSet{one, two},
                  .call_kind = CallKind::callsite(),
              }),
      }));

  taint.add(test::make_taint_config(
      /* kind */ context.kind_factory->get("IndirectSource"),
      test::FrameProperties{
          .callee_port = AccessPath(Root(Root::Kind::Return)),
          .callee = two,
          .call_position = context.positions->unknown(),
          .distance = 3,
          .origins = MethodSet{two},
          .call_kind = CallKind::callsite(),
      }));
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("IndirectSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = one,
                  .call_position = context.positions->unknown(),
                  .distance = 2,
                  .origins = MethodSet{one, two},
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("IndirectSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = two,
                  .call_position = context.positions->unknown(),
                  .distance = 3,
                  .origins = MethodSet{two},
                  .call_kind = CallKind::callsite(),
              }),
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
  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");
  auto* feature_three = context.feature_factory->get("FeatureThree");
  auto* user_feature_one = context.feature_factory->get("UserFeatureOne");
  auto* user_feature_two = context.feature_factory->get("UserFeatureTwo");
  auto* user_feature_three = context.feature_factory->get("UserFeatureThree");

  Taint taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two},
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_two},
              .call_kind = CallKind::callsite(),
          }),
  };
  taint.difference_with(Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two},
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_two},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three},
              .inferred_features = FeatureMayAlwaysSet{feature_three},
              .user_features = FeatureSet{user_feature_three},
              .call_kind = CallKind::callsite(),
          }),
  });
  EXPECT_TRUE(taint.is_bottom());

  taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 2,
              .origins = MethodSet{one},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three},
              .call_kind = CallKind::callsite(),
          }),
  };
  taint.difference_with(Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two},
              .call_kind = CallKind::callsite(),
          }),
  });
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{two},
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = three,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{three},
                  .call_kind = CallKind::callsite(),
              }),
      }));

  taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("SomeOtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two},
              .call_kind = CallKind::callsite(),
          }),
  };
  taint.difference_with(Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = one,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{one},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = two,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{two},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three},
              .call_kind = CallKind::callsite(),
          }),
  });
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("SomeOtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = two,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{two},
                  .call_kind = CallKind::callsite(),
              }),
      }));
}

TEST_F(TaintTest, SetLeafOriginsIfEmpty) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* two =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto taint = Taint{
      // Only the "TestSource" frame should be affected (it is a leaf with empty
      // origins)
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource2"),
          test::FrameProperties{.origins = MethodSet{two}}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource3"),
          test::FrameProperties{
              .callee = two, .call_kind = CallKind::callsite()}),
  };
  taint.set_leaf_origins_if_empty(MethodSet{one});
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{.origins = MethodSet{one}}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource2"),
              test::FrameProperties{.origins = MethodSet{two}}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource3"),
              test::FrameProperties{
                  .callee = two, .call_kind = CallKind::callsite()}),
      }));
}

TEST_F(TaintTest, SetFieldOriginsIfEmpty) {
  Scope scope;
  const auto* field_one = redex::create_field(
      scope, "LClassA", {"field_one", type::java_lang_String()});
  const auto* field_two = redex::create_field(
      scope, "LClassB", {"field_two", type::java_lang_String()});

  DexStore store("stores");
  store.add_classes(scope);
  auto context = test::make_context(store);

  auto* one = context.fields->get(field_one);
  auto* two = context.fields->get(field_two);

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource2"),
          test::FrameProperties{.field_origins = FieldSet{two}}),
  };

  taint.set_field_origins_if_empty(one);
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{.field_origins = FieldSet{one}}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource2"),
              test::FrameProperties{.field_origins = FieldSet{two}}),
      }));
}

TEST_F(TaintTest, LocallyInferredFeatures) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* one =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));

  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");

  // Basic case with only one frame.
  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = one,
              .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
              .call_kind = CallKind::callsite()}),
  };
  EXPECT_EQ(
      taint.locally_inferred_features(
          /* callee */ one,
          /* call_kind */ CallKind::callsite(),
          /* position */ nullptr,
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf))),
      FeatureMayAlwaysSet{feature_one});

  // Port does not have features
  EXPECT_EQ(
      taint.locally_inferred_features(
          /* callee */ one,
          /* call_kind */ CallKind::callsite(),
          /* position */ nullptr,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument))),
      FeatureMayAlwaysSet::bottom());

  // Position does not have features
  EXPECT_EQ(
      taint.locally_inferred_features(
          /* callee */ one,
          /* call_kind */ CallKind::callsite(),
          /* position */ context.positions->unknown(),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf))),
      FeatureMayAlwaysSet::bottom());

  // Frames with different call info.
  taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = nullptr,
              .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
              .call_kind = CallKind::callsite()}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = nullptr,
              .locally_inferred_features = FeatureMayAlwaysSet{feature_two},
              .call_kind = CallKind::origin()}),
  };
  EXPECT_EQ(
      taint.locally_inferred_features(
          /* callee */ nullptr,
          /* call_kind */ CallKind::callsite(),
          /* position */ nullptr,
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf))),
      FeatureMayAlwaysSet{feature_one});
  EXPECT_EQ(
      taint.locally_inferred_features(
          /* callee */ nullptr,
          /* call_kind */ CallKind::origin(),
          /* position */ nullptr,
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf))),
      FeatureMayAlwaysSet{feature_two});

  // Frames with different callee.
  taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = one,
              .locally_inferred_features = FeatureMayAlwaysSet{feature_one},
              .call_kind = CallKind::callsite()}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = nullptr,
              .locally_inferred_features = FeatureMayAlwaysSet{feature_two},
              .call_kind = CallKind::callsite()}),
  };
  EXPECT_EQ(
      taint.locally_inferred_features(
          /* callee */ one,
          /* call_kind */ CallKind::callsite(),
          /* position */ nullptr,
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf))),
      FeatureMayAlwaysSet{feature_one});
  EXPECT_EQ(
      taint.locally_inferred_features(
          /* callee */ nullptr,
          /* call_kind */ CallKind::callsite(),
          /* position */ nullptr,
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf))),
      FeatureMayAlwaysSet{feature_two});
  EXPECT_EQ(
      taint.locally_inferred_features(
          /* callee */ one,
          /* call_kind */ CallKind::origin(),
          /* position */ nullptr,
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf))),
      FeatureMayAlwaysSet::bottom());
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
  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");
  auto* feature_three = context.feature_factory->get("FeatureThree");
  auto* user_feature_one = context.feature_factory->get("UserFeatureOne");
  auto* user_feature_two = context.feature_factory->get("UserFeatureTwo");

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .origins = MethodSet{one},
              .user_features = FeatureSet{user_feature_one},
              .call_kind = CallKind::origin()}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = two,
              .call_position = test_position,
              .distance = 2,
              .origins = MethodSet{two},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one},
              .call_kind = CallKind::callsite()}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .locally_inferred_features = FeatureMayAlwaysSet{feature_two},
              .user_features = FeatureSet{user_feature_one, user_feature_two},
              .call_kind = CallKind::callsite()})};

  // When propagating, all user features become inferred features.
  EXPECT_EQ(
      taint.propagate(
          /* callee */ four,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 2)),
          /* call_position */ context.positions->get("Test.java", 1),
          /* maximum_source_sink_distance */ 100,
          /* extra_features */ FeatureMayAlwaysSet{feature_three},
          /* context */ context,
          /* source_register_types */ {},
          /* source_constant_arguments */ {},
          CallClassIntervalContext(),
          ClassIntervals::Interval::top()),
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 2)),
                  .callee = four,
                  .call_position = context.positions->get("Test.java", 1),
                  .distance = 1,
                  .origins = MethodSet{one},
                  .inferred_features = FeatureMayAlwaysSet{user_feature_one},
                  .locally_inferred_features =
                      FeatureMayAlwaysSet{feature_three},
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
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
                      FeatureMayAlwaysSet{feature_three},
                  .call_kind = CallKind::callsite()}),
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
  auto* feature_one = context.feature_factory->get("FeatureOne");
  auto* feature_two = context.feature_factory->get("FeatureTwo");
  auto* user_feature_one = context.feature_factory->get("UserFeatureOne");
  auto* user_feature_two = context.feature_factory->get("UserFeatureTwo");

  auto* test_source = context.kind_factory->get("TestSource");
  auto* transformed_test_source =
      context.kind_factory->get("TransformedTestSource");
  auto* transformed_test_source2 =
      context.kind_factory->get("TransformedTestSource2");

  auto initial_taint = Taint{
      test::make_taint_config(
          /* kind */ test_source,
          test::FrameProperties{
              .origins = MethodSet{one},
              .user_features = FeatureSet{user_feature_one}}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = two,
              .call_position = test_position,
              .distance = 2,
              .origins = MethodSet{two},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .user_features = FeatureSet{user_feature_one},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
              .callee = three,
              .call_position = test_position,
              .distance = 1,
              .origins = MethodSet{three},
              .inferred_features =
                  FeatureMayAlwaysSet{feature_one, feature_two},
              .user_features = FeatureSet{user_feature_one, user_feature_two},
              .call_kind = CallKind::callsite(),
          }),
  };

  // This works the same way as filter.
  auto empty_taint = initial_taint;
  empty_taint.transform_kind_with_features(
      [](const auto* /* unused kind */) { return std::vector<const Kind*>(); },
      [](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet::bottom();
      });
  EXPECT_EQ(empty_taint, Taint::bottom());

  // This actually performs a transformation.
  auto map_test_source_taint = initial_taint;
  map_test_source_taint.transform_kind_with_features(
      [test_source,
       transformed_test_source](const auto* kind) -> std::vector<const Kind*> {
        if (kind == test_source) {
          return {transformed_test_source};
        }
        return {kind};
      },
      [](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet::bottom();
      });
  EXPECT_EQ(
      map_test_source_taint,
      (Taint{
          test::make_taint_config(
              /* kind */ transformed_test_source,
              test::FrameProperties{
                  .origins = MethodSet{one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .callee = two,
                  .call_position = test_position,
                  .distance = 2,
                  .origins = MethodSet{two},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one},
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = three,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{three},
                  .inferred_features =
                      FeatureMayAlwaysSet{feature_one, feature_two},
                  .user_features =
                      FeatureSet{user_feature_one, user_feature_two},
                  .call_kind = CallKind::callsite(),
              }),
      }));

  // Another transformation. Covers mapping transformed frames.
  map_test_source_taint = initial_taint;
  map_test_source_taint.transform_kind_with_features(
      [test_source, transformed_test_source](const auto* kind) {
        if (kind == test_source) {
          return std::vector<const Kind*>{transformed_test_source};
        }
        return std::vector<const Kind*>{kind};
      },
      [feature_two](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet{feature_two};
      });
  EXPECT_EQ(
      map_test_source_taint,
      (Taint{
          test::make_taint_config(
              /* kind */ transformed_test_source,
              test::FrameProperties{
                  .origins = MethodSet{one},
                  .inferred_features = FeatureMayAlwaysSet{feature_two},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
                  .callee = two,
                  .call_position = test_position,
                  .distance = 2,
                  .origins = MethodSet{two},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one},
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("OtherSource"),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Argument, 0)),
                  .callee = three,
                  .call_position = test_position,
                  .distance = 1,
                  .origins = MethodSet{three},
                  .inferred_features =
                      FeatureMayAlwaysSet{feature_one, feature_two},
                  .user_features =
                      FeatureSet{user_feature_one, user_feature_two},
                  .call_kind = CallKind::callsite(),
              }),
      }));

  // Tests one -> many transformations (with features).
  map_test_source_taint = initial_taint;
  map_test_source_taint.transform_kind_with_features(
      [test_source, transformed_test_source, transformed_test_source2](
          const auto* kind) {
        if (kind == test_source) {
          return std::vector<const Kind*>{
              test_source, transformed_test_source, transformed_test_source2};
        }
        return std::vector<const Kind*>{};
      },
      [feature_one](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet{feature_one};
      });
  EXPECT_EQ(
      map_test_source_taint,
      (Taint{
          test::make_taint_config(
              /* kind */ test_source,
              test::FrameProperties{
                  .origins = MethodSet{one},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              /* kind */ transformed_test_source,
              test::FrameProperties{
                  .origins = MethodSet{one},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              /* kind */ transformed_test_source2,
              test::FrameProperties{
                  .origins = MethodSet{one},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
      }));

  // Tests transformations with features added to specific kinds.
  map_test_source_taint = initial_taint;
  map_test_source_taint.transform_kind_with_features(
      [test_source, transformed_test_source, transformed_test_source2](
          const auto* kind) {
        if (kind == test_source) {
          return std::vector<const Kind*>{
              transformed_test_source, transformed_test_source2};
        }
        return std::vector<const Kind*>{};
      },
      [&](const auto* transformed_kind) {
        if (transformed_kind == transformed_test_source) {
          return FeatureMayAlwaysSet{feature_one};
        }
        return FeatureMayAlwaysSet::bottom();
      });
  EXPECT_EQ(
      map_test_source_taint,
      (Taint{
          test::make_taint_config(
              /* kind */ transformed_test_source,
              test::FrameProperties{
                  .origins = MethodSet{one},
                  .inferred_features = FeatureMayAlwaysSet{feature_one},
                  .user_features = FeatureSet{user_feature_one}}),
          test::make_taint_config(
              /* kind */ transformed_test_source2,
              test::FrameProperties{
                  .origins = MethodSet{one},
                  .user_features = FeatureSet{user_feature_one}}),
      }));

  // Transformation where multiple old kinds map to the same new kind
  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource1"),
          test::FrameProperties{
              .callee = two,
              .call_position = test_position,
              .origins = MethodSet{two},
              .inferred_features = FeatureMayAlwaysSet{feature_one},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("OtherSource2"),
          test::FrameProperties{
              .callee = two,
              .call_position = test_position,
              .origins = MethodSet{three},
              .inferred_features = FeatureMayAlwaysSet{feature_two},
              .call_kind = CallKind::callsite(),
          }),
  };
  taint.transform_kind_with_features(
      [&](const auto* /* unused kind */) -> std::vector<const Kind*> {
        return {transformed_test_source};
      },
      [](const auto* /* unused kind */) {
        return FeatureMayAlwaysSet::bottom();
      });
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              transformed_test_source,
              test::FrameProperties{
                  .callee = two,
                  .call_position = test_position,
                  .origins = MethodSet{two, three},
                  .inferred_features = FeatureMayAlwaysSet(
                      /* may */ FeatureSet{feature_one, feature_two},
                      /* always */ FeatureSet{}),
                  .call_kind = CallKind::callsite(),
              }),
      }));
}

TEST_F(TaintTest, AppendOutputPaths) {
  auto context = test::make_empty_context();

  const auto path_element1 = PathElement::field("field1");
  const auto path_element2 = PathElement::field("field2");

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{}),
      test::make_taint_config(
          context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .output_paths =
                  PathTreeDomain{{Path{path_element1}, CollapseDepth(4)}},
              .call_kind = CallKind::propagation(),
          })};

  taint.append_to_propagation_output_paths(path_element2);
  EXPECT_EQ(
      taint,
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource"),
              test::FrameProperties{}),
          test::make_taint_config(
              context.kind_factory->local_return(),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .output_paths =
                      PathTreeDomain{
                          {Path{path_element1, path_element2},
                           CollapseDepth(3)}},
                  .call_kind = CallKind::propagation(),
              })}));
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

  auto dex_position1 =
      DexPosition(DexString::make_string("UnknownSource"), /* line */ 1);
  auto dex_position2 =
      DexPosition(DexString::make_string("UnknownSource"), /* line */ 2);
  auto dex_position3 =
      DexPosition(DexString::make_string("UnknownSource"), /* line */ 3);

  auto position1 = context.positions->get(method1, &dex_position1);
  auto position2 = context.positions->get(method2, &dex_position2);
  auto position3 = context.positions->get(method2, &dex_position3);

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("LeafFrame"),
          test::FrameProperties{}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("NonLeafFrame1"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = method1,
              .call_position = position1,
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("NonLeafFrame2"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .callee = method2,
              .call_position = position2,
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("NonLeafFrame3"),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument, 1)),
              .callee = method3,
              .call_position = position3,
              .call_kind = CallKind::callsite(),
          })};

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
  EXPECT_EQ(taint.local_positions(), LocalPositionSet{position1});

  // Verify that local positions were updated only in non-leaf frames.
  auto partitioned_is_leaf =
      taint.partition_by_kind<bool>([&](const Kind* kind) {
        return kind == context.kind_factory->get("LeafFrame");
      });
  EXPECT_EQ(partitioned_is_leaf[true].local_positions(), LocalPositionSet{});
  EXPECT_EQ(
      partitioned_is_leaf[false].local_positions(),
      LocalPositionSet{position1});

  // Verify that call positions were updated only in non-leaf frames.
  for (const auto& frame : taint.frames_iterator()) {
    if (frame.callee() == method1) {
      EXPECT_EQ(
          frame.call_position(),
          context.positions->get(
              position1, /* line */ 10, /* start */ 11, /* end */ 12));
    } else if (frame.callee() == method2) {
      EXPECT_EQ(
          frame.call_position(),
          context.positions->get(
              position2, /* line */ 20, /* start */ 21, /* end */ 22));
    } else if (frame.callee() == method3) {
      EXPECT_EQ(frame.call_position(), position3);
    } else if (frame.callee() == nullptr) {
      EXPECT_EQ(frame.call_position(), nullptr);
    } else {
      // There should be no other frames.
      EXPECT_TRUE(false);
    }
  }
}

TEST_F(TaintTest, FilterInvalidFrames) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method1 =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* test_kind_one = context.kind_factory->get("TestSourceOne");
  auto* test_kind_two = context.kind_factory->get("TestSourceTwo");

  // Filter by callee
  auto taint = Taint{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .callee = method1,
              .call_kind = CallKind::callsite(),
          })};
  taint.filter_invalid_frames(
      /* is_valid */
      [&](const Method* MT_NULLABLE callee,
          const AccessPath& /* callee_port */,
          const Kind* /* kind */) { return callee == nullptr; });
  EXPECT_EQ(
      taint,
      (Taint{test::make_taint_config(test_kind_one, test::FrameProperties{})}));

  // Filter by callee port
  taint = Taint{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .callee = method1,
              .call_kind = CallKind::callsite(),
          })};
  taint.filter_invalid_frames(
      /* is_valid */
      [&](const Method* MT_NULLABLE /* callee */,
          const AccessPath& callee_port,
          const Kind* /* kind */) {
        return callee_port == AccessPath(Root(Root::Kind::Argument));
      });
  EXPECT_EQ(
      taint,
      (Taint{test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .callee = method1,
              .call_kind = CallKind::callsite(),
          })}));
  // Filter by kind
  taint = Taint{
      test::make_taint_config(test_kind_one, test::FrameProperties{}),
      test::make_taint_config(
          test_kind_two,
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Argument)),
              .callee = method1,
              .call_kind = CallKind::callsite(),
          })};
  taint.filter_invalid_frames(
      /* is_valid */
      [&](const Method* MT_NULLABLE /* callee */,
          const AccessPath& /* callee_port */,
          const Kind* kind) { return kind != test_kind_two; });
  EXPECT_EQ(
      taint,
      (Taint{test::make_taint_config(test_kind_one, test::FrameProperties{})}));
}

TEST_F(TaintTest, ContainsKind) {
  auto context = test::make_empty_context();

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{}),
      test::make_taint_config(
          /* kind */ context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              .call_kind = CallKind::propagation(),
          })};

  EXPECT_TRUE(taint.contains_kind(context.kind_factory->local_return()));
  EXPECT_TRUE(taint.contains_kind(context.kind_factory->get("TestSource")));
  EXPECT_FALSE(taint.contains_kind(context.kind_factory->get("TestSink")));
}

TEST_F(TaintTest, PartitionByKind) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method1 =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* method2 =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource1"),
          test::FrameProperties{}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource2"),
          test::FrameProperties{}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource3"),
          test::FrameProperties{
              .callee = method1,
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource3"),
          test::FrameProperties{
              .callee = method2,
              .call_kind = CallKind::callsite(),
          })};

  auto taint_by_kind = taint.partition_by_kind();
  EXPECT_TRUE(taint_by_kind.size() == 3);
  EXPECT_EQ(
      taint_by_kind[context.kind_factory->get("TestSource1")],
      Taint{test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource1"),
          test::FrameProperties{})});
  EXPECT_EQ(
      taint_by_kind[context.kind_factory->get("TestSource2")],
      Taint{test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource2"),
          test::FrameProperties{})});
  EXPECT_EQ(
      taint_by_kind[context.kind_factory->get("TestSource3")],
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource3"),
              test::FrameProperties{
                  .callee = method1,
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource3"),
              test::FrameProperties{
                  .callee = method2,
                  .call_kind = CallKind::callsite(),
              })}));
}

TEST_F(TaintTest, PartitionByKindGeneric) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method1 =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* method2 =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              .call_kind = CallKind::propagation(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->local_return(),
          test::FrameProperties{
              .callee_port = AccessPath(Root(Root::Kind::Return)),
              .callee = method1,
              .output_paths = PathTreeDomain{{Path{}, CollapseDepth::zero()}},
              .call_kind = CallKind::propagation_with_trace(CallKind::CallSite),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource1"),
          test::FrameProperties{
              .callee = method1,
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource2"),
          test::FrameProperties{
              .callee = method2,
              .call_kind = CallKind::callsite(),
          })};

  auto taint_by_kind = taint.partition_by_kind<bool>([&](const Kind* kind) {
    return kind->discard_transforms()->is<PropagationKind>();
  });
  EXPECT_TRUE(taint_by_kind.size() == 2);
  EXPECT_EQ(
      taint_by_kind[true],
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->local_return(),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .output_paths =
                      PathTreeDomain{{Path{}, CollapseDepth::zero()}},
                  .call_kind = CallKind::propagation(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->local_return(),
              test::FrameProperties{
                  .callee_port = AccessPath(Root(Root::Kind::Return)),
                  .callee = method1,
                  .output_paths =
                      PathTreeDomain{{Path{}, CollapseDepth::zero()}},
                  .call_kind =
                      CallKind::propagation_with_trace(CallKind::CallSite),
              }),
      }));
  EXPECT_EQ(
      taint_by_kind[false],
      (Taint{
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource1"),
              test::FrameProperties{
                  .callee = method1,
                  .call_kind = CallKind::callsite(),
              }),
          test::make_taint_config(
              /* kind */ context.kind_factory->get("TestSource2"),
              test::FrameProperties{
                  .callee = method2,
                  .call_kind = CallKind::callsite(),
              }),
      }));
}

TEST_F(TaintTest, IntersectIntervals) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method1 =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* method2 =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto initial_taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = method1,
              .class_interval_context = CallClassIntervalContext(
                  ClassIntervals::Interval::finite(2, 6),
                  /* preserves_type_context*/ true),
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = method2,
              .class_interval_context = CallClassIntervalContext(
                  ClassIntervals::Interval::finite(4, 5),
                  /* preserves_type_context*/ false),
              .call_kind = CallKind::callsite(),
          })};

  {
    // Frames with preserves_type_context = false are always preserved even if
    // they intersect with nothing.
    auto taint = initial_taint;
    taint.intersect_intervals_with(Taint::bottom());
    EXPECT_EQ(
        taint,
        Taint{test::make_taint_config(
            /* kind */ context.kind_factory->get("TestSource"),
            test::FrameProperties{
                .callee = method2,
                .class_interval_context = CallClassIntervalContext(
                    ClassIntervals::Interval::finite(4, 5),
                    /* preserves_type_context*/ false),
                .call_kind = CallKind::callsite(),
            })});
  }

  {
    // Drop frames with preserves_type_context=true that don't intersect with
    // anything.
    auto taint = initial_taint;
    taint.intersect_intervals_with(Taint{test::make_taint_config(
        /* kind */ context.kind_factory->get("TestSource"),
        test::FrameProperties{
            .callee = method2,
            .class_interval_context = CallClassIntervalContext(
                ClassIntervals::Interval::finite(10, 11),
                /* preserves_type_context*/ true),
            .call_kind = CallKind::callsite(),
        })});
    EXPECT_EQ(
        taint,
        Taint{test::make_taint_config(
            /* kind */ context.kind_factory->get("TestSource"),
            test::FrameProperties{
                .callee = method2,
                .class_interval_context = CallClassIntervalContext(
                    ClassIntervals::Interval::finite(4, 5),
                    /* preserves_type_context*/ false),
                .call_kind = CallKind::callsite(),
            })});
  }

  {
    // Taint is never "expanded" as a result of the intersection.
    auto taint = Taint::bottom();
    taint.intersect_intervals_with(initial_taint);
    EXPECT_EQ(taint, Taint::bottom());
  }

  {
    // Taint intersecting with itself gives itself
    auto taint = initial_taint;
    taint.intersect_intervals_with(initial_taint);
    EXPECT_EQ(taint, initial_taint);
  }

  {
    // Produce bottom() if nothing intersects
    auto taint = Taint{test::make_taint_config(
        /* kind */ context.kind_factory->get("TestSource"),
        test::FrameProperties{
            .callee = method2,
            .class_interval_context = CallClassIntervalContext(
                ClassIntervals::Interval::finite(4, 5),
                /* preserves_type_context*/ true),
            .call_kind = CallKind::callsite(),
        })};
    taint.intersect_intervals_with(Taint{test::make_taint_config(
        /* kind */ context.kind_factory->get("TestSource"),
        test::FrameProperties{
            .callee = method2,
            .class_interval_context = CallClassIntervalContext(
                ClassIntervals::Interval::finite(6, 7),
                /* preserves_type_context*/ true),
            .call_kind = CallKind::callsite(),
        })});
    EXPECT_TRUE(taint.is_bottom());
  }

  {
    // If a frame is kept during intersection, all properties are kept the same
    auto taint = initial_taint;
    taint.intersect_intervals_with(Taint{test::make_taint_config(
        /* kind */ context.kind_factory->get("TestSource"),
        test::FrameProperties{
            .callee = method2, // method1 in initial_taint
            .class_interval_context = CallClassIntervalContext(
                ClassIntervals::Interval::finite(
                    2, 3), // (2, 6) in initial_taint
                /* preserves_type_context*/ true),
            .call_kind = CallKind::callsite(),
        })});
    EXPECT_EQ(taint, initial_taint);
  }

  {
    // Taint is preserved as long as it intersects with least one frame
    auto taint = initial_taint;
    taint.intersect_intervals_with(Taint{
        test::make_taint_config(
            /* kind */ context.kind_factory->get("TestSource"),
            test::FrameProperties{
                .callee = method1,
                .class_interval_context = CallClassIntervalContext(
                    ClassIntervals::Interval::finite(2, 3),
                    /* preserves_type_context*/ true),
                .call_kind = CallKind::callsite(),
            }),
        test::make_taint_config(
            /* kind */ context.kind_factory->get("TestSource"),
            test::FrameProperties{
                .callee = method1,
                .class_interval_context = CallClassIntervalContext(
                    ClassIntervals::Interval::finite(10, 11),
                    /* preserves_type_context*/ true),
                .call_kind = CallKind::callsite(),
            }),
    });
    EXPECT_EQ(taint, initial_taint);
  }

  {
    // If other has at least one frame with preserves_type_context=false, all
    // frames are retained (because everything intersects with it).
    auto taint = initial_taint;
    taint.intersect_intervals_with(Taint{
        test::make_taint_config(
            /* kind */ context.kind_factory->get("TestSource"),
            test::FrameProperties{
                .callee = method2,
                .class_interval_context = CallClassIntervalContext(
                    ClassIntervals::Interval::top(),
                    /* preserves_type_context*/ false),
                .call_kind = CallKind::callsite(),
            }),
        test::make_taint_config(
            /* kind */ context.kind_factory->get("TestSource"),
            test::FrameProperties{
                .callee = method1,
                .class_interval_context = CallClassIntervalContext(
                    ClassIntervals::Interval::finite(
                        10, 11), // intersects with nothing in initial_taint
                    /* preserves_type_context*/ true),
                .call_kind = CallKind::callsite(),
            }),
    });
    EXPECT_EQ(taint, initial_taint);
  }
}

TEST_F(TaintTest, FeaturesJoined) {
  auto context = test::make_empty_context();

  Scope scope;
  auto* method1 =
      context.methods->create(redex::create_void_method(scope, "LOne;", "one"));
  auto* method2 =
      context.methods->create(redex::create_void_method(scope, "LTwo;", "two"));

  auto* feature1 = context.feature_factory->get("Feature1");
  auto* feature2 = context.feature_factory->get("Feature2");
  auto* feature3 = context.feature_factory->get("Feature3");

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = method1,
              .inferred_features = FeatureMayAlwaysSet{feature1},
              .call_kind = CallKind::callsite(),
          }),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource"),
          test::FrameProperties{
              .callee = method2,
              .inferred_features = FeatureMayAlwaysSet(
                  /* may */ FeatureSet{feature2},
                  /* always */ FeatureSet{feature3}),
              .locally_inferred_features = FeatureMayAlwaysSet{feature1},
              .call_kind = CallKind::callsite(),
          })};

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

TEST_F(TaintTest, FramesIterator) {
  auto context = test::make_empty_context();

  auto taint = Taint{
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource1"),
          test::FrameProperties{}),
      test::make_taint_config(
          /* kind */ context.kind_factory->get("TestSource2"),
          test::FrameProperties{})};

  std::unordered_set<const Kind*> kinds;
  for (const auto& frame : taint.frames_iterator()) {
    kinds.insert(frame.kind());
  }

  EXPECT_EQ(kinds.size(), 2);
  EXPECT_NE(kinds.find(context.kind_factory->get("TestSource1")), kinds.end());
  EXPECT_NE(kinds.find(context.kind_factory->get("TestSource2")), kinds.end());
}

} // namespace marianatrench

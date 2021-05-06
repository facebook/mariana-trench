/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

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

  taint.add(Frame(
      /* kind */ context.kinds->get("TestSource"),
      /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
      /* callee */ nullptr,
      /* call_position */ nullptr,
      /* distance */ 0,
      /* origins */ {},
      /* inferred_features */ {},
      /* locally_inferred_features */ {},
      /* user_features */ {},
      /* via_type_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {}));
  EXPECT_EQ(
      taint,
      (Taint{
          Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
      }));

  taint.add(Frame(
      /* kind */ context.kinds->get("OtherSource"),
      /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
      /* callee */ nullptr,
      /* call_position */ nullptr,
      /* distance */ 0,
      /* origins */ {},
      /* inferred_features */ {},
      /* locally_inferred_features */ {},
      /* user_features */ {},
      /* via_type_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {}));
  EXPECT_EQ(
      taint,
      (Taint{
          Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ context.kinds->get("OtherSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
      }));

  taint.add(Frame(
      /* kind */ context.kinds->get("IndirectSource"),
      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
      /* callee */ one,
      /* call_position */ context.positions->unknown(),
      /* distance */ 2,
      /* origins */ MethodSet{one},
      /* inferred_features */ {},
      /* locally_inferred_features */ {},
      /* user_features */ {},
      /* via_type_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {}));
  EXPECT_EQ(
      taint,
      (Taint{
          Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ context.kinds->get("OtherSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ context.kinds->get("IndirectSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 2,
              /* origins */ MethodSet{one},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
      }));

  taint.add(Frame(
      /* kind */ context.kinds->get("IndirectSource"),
      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
      /* callee */ one,
      /* call_position */ context.positions->unknown(),
      /* distance */ 3,
      /* origins */ MethodSet{two},
      /* inferred_features */ {},
      /* locally_inferred_features */ {},
      /* user_features */ {},
      /* via_type_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {}));
  EXPECT_EQ(
      taint,
      (Taint{
          Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ context.kinds->get("OtherSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ context.kinds->get("IndirectSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 2,
              /* origins */ MethodSet{one, two},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
      }));

  taint.add(Frame(
      /* kind */ context.kinds->get("IndirectSource"),
      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
      /* callee */ two,
      /* call_position */ context.positions->unknown(),
      /* distance */ 3,
      /* origins */ MethodSet{two},
      /* inferred_features */ {},
      /* locally_inferred_features */ {},
      /* user_features */ {},
      /* via_type_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {}));
  EXPECT_EQ(
      taint,
      (Taint{
          Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ context.kinds->get("OtherSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ context.kinds->get("IndirectSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 2,
              /* origins */ MethodSet{one, two},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ context.kinds->get("IndirectSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ two,
              /* call_position */ context.positions->unknown(),
              /* distance */ 3,
              /* origins */ MethodSet{two},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
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
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ one,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* inferred_features */ FeatureMayAlwaysSet{feature_one},
          /* locally_inferred_features */ {},
          /* user_features */ FeatureSet{user_feature_one},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ context.kinds->get("OtherSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ two,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{two},
          /* inferred_features */ FeatureMayAlwaysSet{feature_two},
          /* locally_inferred_features */ {},
          /* user_features */ FeatureSet{user_feature_two},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
  };
  taint.difference_with(Taint{
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ one,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* inferred_features */ FeatureMayAlwaysSet{feature_one},
          /* locally_inferred_features */ {},
          /* user_features */ FeatureSet{user_feature_one},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ context.kinds->get("OtherSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ two,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{two},
          /* inferred_features */ FeatureMayAlwaysSet{feature_two},
          /* locally_inferred_features */ {},
          /* user_features */ FeatureSet{user_feature_two},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ context.kinds->get("OtherSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ three,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{three},
          /* inferred_features */ FeatureMayAlwaysSet{feature_three},
          /* locally_inferred_features */ {},
          /* user_features */ FeatureSet{user_feature_three},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
  });
  EXPECT_TRUE(taint.is_bottom());

  taint = Taint{
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ one,
          /* call_position */ test_position,
          /* distance */ 2,
          /* origins */ MethodSet{one},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ context.kinds->get("OtherSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ two,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{two},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ three,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{three},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
  };
  taint.difference_with(Taint{
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ one,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ two,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{two},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
  });
  EXPECT_EQ(
      taint,
      (Taint{
          Frame(
              /* kind */ context.kinds->get("OtherSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
              /* callee */ two,
              /* call_position */ test_position,
              /* distance */ 1,
              /* origins */ MethodSet{two},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
              /* callee */ three,
              /* call_position */ test_position,
              /* distance */ 1,
              /* origins */ MethodSet{three},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
      }));

  taint = Taint{
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* callee */ one,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ context.kinds->get("SomeOtherSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ two,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{two},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
  };
  taint.difference_with(Taint{
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* callee */ one,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ two,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{two},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ three,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{three},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
  });
  EXPECT_EQ(
      taint,
      (Taint{
          Frame(
              /* kind */ context.kinds->get("SomeOtherSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
              /* callee */ two,
              /* call_position */ test_position,
              /* distance */ 1,
              /* origins */ MethodSet{two},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
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
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* callee */ one,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* inferred_features */ FeatureMayAlwaysSet{feature_one},
          /* locally_inferred_features */ {},
          /* user_features */ FeatureSet{user_feature_one},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ two,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{two},
          /* inferred_features */ FeatureMayAlwaysSet{feature_two},
          /* locally_inferred_features */ {},
          /* user_features */ FeatureSet{user_feature_two},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ three,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{three},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
  };
  taint.add_inferred_features(
      FeatureMayAlwaysSet::make_always({feature_three}));
  EXPECT_EQ(
      taint,
      (Taint{
          Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 1)),
              /* callee */ one,
              /* call_position */ test_position,
              /* distance */ 1,
              /* origins */ MethodSet{one},
              /* inferred_features */ FeatureMayAlwaysSet{feature_one},
              /* locally_inferred_features */
              FeatureMayAlwaysSet{feature_three},
              /* user_features */ FeatureSet{user_feature_one},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
              /* callee */ two,
              /* call_position */ test_position,
              /* distance */ 1,
              /* origins */ MethodSet{two},
              /* inferred_features */ FeatureMayAlwaysSet{feature_two},
              /* locally_inferred_features */
              FeatureMayAlwaysSet{feature_three},
              /* user_features */ FeatureSet{user_feature_two},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
              /* callee */ three,
              /* call_position */ test_position,
              /* distance */ 1,
              /* origins */ MethodSet{three},
              /* inferred_features */ {},
              /* locally_inferred_features */
              FeatureMayAlwaysSet{feature_three},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
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
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ MethodSet{one},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ FeatureSet{user_feature_one},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ context.kinds->get("OtherSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* callee */ two,
          /* call_position */ test_position,
          /* distance */ 2,
          /* origins */ MethodSet{two},
          /* inferred_features */ FeatureMayAlwaysSet{feature_one},
          /* locally_inferred_features */ {},
          /* user_features */ FeatureSet{user_feature_one},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ context.kinds->get("OtherSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ three,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{three},
          /* inferred_features */ FeatureMayAlwaysSet{feature_one},
          /* locally_inferred_features */ FeatureMayAlwaysSet{feature_two},
          /* user_features */ FeatureSet{user_feature_one, user_feature_two},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
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
          /* source_register_types */ {}),
      (Taint{
          Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 2)),
              /* callee */ four,
              /* call_position */ context.positions->get("Test.java", 1),
              /* distance */ 1,
              /* origins */ MethodSet{one},
              /* inferred_features */ FeatureMayAlwaysSet{user_feature_one},
              /* locally_inferred_features */
              FeatureMayAlwaysSet{feature_three},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ context.kinds->get("OtherSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 2)),
              /* callee */ four,
              /* call_position */ context.positions->get("Test.java", 1),
              /* distance */ 2,
              /* origins */ MethodSet{two, three},
              /* inferred_features */
              FeatureMayAlwaysSet(
                  /* may */ FeatureSet{user_feature_two, feature_two},
                  /* always */
                  FeatureSet{user_feature_one, feature_one}),
              /* locally_inferred_features */
              FeatureMayAlwaysSet{feature_three},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
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
      Frame(
          /* kind */ test_source,
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ MethodSet{one},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ FeatureSet{user_feature_one},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ context.kinds->get("OtherSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* callee */ two,
          /* call_position */ test_position,
          /* distance */ 2,
          /* origins */ MethodSet{two},
          /* inferred_features */ FeatureMayAlwaysSet{feature_one},
          /* locally_inferred_features */ {},
          /* user_features */ FeatureSet{user_feature_one},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ context.kinds->get("OtherSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ three,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{three},
          /* inferred_features */ FeatureMayAlwaysSet{feature_one, feature_two},
          /* locally_inferred_features */ {},
          /* user_features */ FeatureSet{user_feature_one, user_feature_two},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
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
          Frame(
              /* kind */ transformed_test_source,
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ MethodSet{one},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ FeatureSet{user_feature_one},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ context.kinds->get("OtherSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 1)),
              /* callee */ two,
              /* call_position */ test_position,
              /* distance */ 2,
              /* origins */ MethodSet{two},
              /* inferred_features */ FeatureMayAlwaysSet{feature_one},
              /* locally_inferred_features */ {},
              /* user_features */ FeatureSet{user_feature_one},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ context.kinds->get("OtherSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
              /* callee */ three,
              /* call_position */ test_position,
              /* distance */ 1,
              /* origins */ MethodSet{three},
              /* inferred_features */
              FeatureMayAlwaysSet{feature_one, feature_two},
              /* locally_inferred_features */ {},
              /* user_features */
              FeatureSet{user_feature_one, user_feature_two},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
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
          Frame(
              /* kind */ transformed_test_source,
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ MethodSet{one},
              /* inferred_features */ {},
              /* locally_inferred_features */ FeatureMayAlwaysSet{feature_one},
              /* user_features */ FeatureSet{user_feature_one},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ context.kinds->get("OtherSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 1)),
              /* callee */ two,
              /* call_position */ test_position,
              /* distance */ 2,
              /* origins */ MethodSet{two},
              /* inferred_features */ FeatureMayAlwaysSet{feature_one},
              /* locally_inferred_features */ {},
              /* user_features */ FeatureSet{user_feature_one},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ context.kinds->get("OtherSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
              /* callee */ three,
              /* call_position */ test_position,
              /* distance */ 1,
              /* origins */ MethodSet{three},
              /* inferred_features */
              FeatureMayAlwaysSet{feature_one, feature_two},
              /* locally_inferred_features */ {},
              /* user_features */
              FeatureSet{user_feature_one, user_feature_two},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
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
          Frame(
              /* kind */ test_source,
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ MethodSet{one},
              /* inferred_features */ {},
              /* locally_inferred_features */ FeatureMayAlwaysSet{feature_one},
              /* user_features */ FeatureSet{user_feature_one},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ transformed_test_source,
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ MethodSet{one},
              /* inferred_features */ {},
              /* locally_inferred_features */ FeatureMayAlwaysSet{feature_one},
              /* user_features */ FeatureSet{user_feature_one},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ transformed_test_source2,
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ MethodSet{one},
              /* inferred_features */ {},
              /* locally_inferred_features */ FeatureMayAlwaysSet{feature_one},
              /* user_features */ FeatureSet{user_feature_one},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
      }));
}

} // namespace marianatrench

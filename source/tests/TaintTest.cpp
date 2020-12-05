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
      /* features */ {},
      /* local_positions */ {}));
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
              /* features */ {},
              /* local_positions */ {}),
      }));

  taint.add(Frame(
      /* kind */ context.kinds->get("OtherSource"),
      /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
      /* callee */ nullptr,
      /* call_position */ nullptr,
      /* distance */ 0,
      /* origins */ {},
      /* features */ {},
      /* local_positions */ {}));
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
              /* features */ {},
              /* local_positions */ {}),
          Frame(
              /* kind */ context.kinds->get("OtherSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* features */ {},
              /* local_positions */ {}),
      }));

  taint.add(Frame(
      /* kind */ context.kinds->get("IndirectSource"),
      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
      /* callee */ one,
      /* call_position */ context.positions->unknown(),
      /* distance */ 2,
      /* origins */ MethodSet{one},
      /* features */ {},
      /* local_positions */ {}));
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
              /* features */ {},
              /* local_positions */ {}),
          Frame(
              /* kind */ context.kinds->get("OtherSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* features */ {},
              /* local_positions */ {}),
          Frame(
              /* kind */ context.kinds->get("IndirectSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 2,
              /* origins */ MethodSet{one},
              /* features */ {},
              /* local_positions */ {}),
      }));

  taint.add(Frame(
      /* kind */ context.kinds->get("IndirectSource"),
      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
      /* callee */ one,
      /* call_position */ context.positions->unknown(),
      /* distance */ 3,
      /* origins */ MethodSet{two},
      /* features */ {},
      /* local_positions */ {}));
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
              /* features */ {},
              /* local_positions */ {}),
          Frame(
              /* kind */ context.kinds->get("OtherSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* features */ {},
              /* local_positions */ {}),
          Frame(
              /* kind */ context.kinds->get("IndirectSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 2,
              /* origins */ MethodSet{one, two},
              /* features */ {},
              /* local_positions */ {}),
      }));

  taint.add(Frame(
      /* kind */ context.kinds->get("IndirectSource"),
      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
      /* callee */ two,
      /* call_position */ context.positions->unknown(),
      /* distance */ 3,
      /* origins */ MethodSet{two},
      /* features */ {},
      /* local_positions */ {}));
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
              /* features */ {},
              /* local_positions */ {}),
          Frame(
              /* kind */ context.kinds->get("OtherSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* features */ {},
              /* local_positions */ {}),
          Frame(
              /* kind */ context.kinds->get("IndirectSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 2,
              /* origins */ MethodSet{one, two},
              /* features */ {},
              /* local_positions */ {}),
          Frame(
              /* kind */ context.kinds->get("IndirectSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ two,
              /* call_position */ context.positions->unknown(),
              /* distance */ 3,
              /* origins */ MethodSet{two},
              /* features */ {},
              /* local_positions */ {}),
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

  Taint taint = Taint{
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ one,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* features */ FeatureMayAlwaysSet{feature_one},
          /* local_positions */ {}),
      Frame(
          /* kind */ context.kinds->get("OtherSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ two,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{two},
          /* features */ FeatureMayAlwaysSet{feature_two},
          /* local_positions */ {}),
  };
  taint.difference_with(Taint{
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ one,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* features */ FeatureMayAlwaysSet{feature_one},
          /* local_positions */ {}),
      Frame(
          /* kind */ context.kinds->get("OtherSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ two,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{two},
          /* features */ FeatureMayAlwaysSet{feature_two},
          /* local_positions */ {}),
      Frame(
          /* kind */ context.kinds->get("OtherSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ three,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{three},
          /* features */ FeatureMayAlwaysSet{feature_three},
          /* local_positions */ {}),
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
          /* features */ {},
          /* local_positions */ {}),
      Frame(
          /* kind */ context.kinds->get("OtherSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ two,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{two},
          /* features */ {},
          /* local_positions */ {}),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ three,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{three},
          /* features */ {},
          /* local_positions */ {}),
  };
  taint.difference_with(Taint{
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ one,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* features */ {},
          /* local_positions */ {}),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ two,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{two},
          /* features */ {},
          /* local_positions */ {}),
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
              /* features */ {},
              /* local_positions */ {}),
          Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
              /* callee */ three,
              /* call_position */ test_position,
              /* distance */ 1,
              /* origins */ MethodSet{three},
              /* features */ {},
              /* local_positions */ {}),
      }));

  taint = Taint{
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* callee */ one,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* features */ {},
          /* local_positions */ {}),
      Frame(
          /* kind */ context.kinds->get("SomeOtherSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ two,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{two},
          /* features */ {},
          /* local_positions */ {}),
  };
  taint.difference_with(Taint{
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* callee */ one,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* features */ {},
          /* local_positions */ {}),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ two,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{two},
          /* features */ {},
          /* local_positions */ {}),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ three,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{three},
          /* features */ {},
          /* local_positions */ {}),
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
              /* features */ {},
              /* local_positions */ {}),
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

  auto taint = Taint{
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* callee */ one,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* features */ FeatureMayAlwaysSet{feature_one},
          /* local_positions */ {}),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ two,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{two},
          /* features */ FeatureMayAlwaysSet{feature_two},
          /* local_positions */ {}),
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ three,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{three},
          /* features */ {},
          /* local_positions */ {}),
  };
  taint.add_features(FeatureMayAlwaysSet::make_always({feature_three}));
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
              /* features */ FeatureMayAlwaysSet{feature_one, feature_three},
              /* local_positions */ {}),
          Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
              /* callee */ two,
              /* call_position */ test_position,
              /* distance */ 1,
              /* origins */ MethodSet{two},
              /* features */ FeatureMayAlwaysSet{feature_two, feature_three},
              /* local_positions */ {}),
          Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
              /* callee */ three,
              /* call_position */ test_position,
              /* distance */ 1,
              /* origins */ MethodSet{three},
              /* features */ FeatureMayAlwaysSet{feature_three},
              /* local_positions */ {}),
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

  auto taint = Taint{
      Frame(
          /* kind */ context.kinds->get("TestSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ MethodSet{one},
          /* features */ {},
          /* local_positions */ {}),
      Frame(
          /* kind */ context.kinds->get("OtherSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 1)),
          /* callee */ two,
          /* call_position */ test_position,
          /* distance */ 2,
          /* origins */ MethodSet{two},
          /* features */ FeatureMayAlwaysSet{feature_one},
          /* local_positions */ {}),
      Frame(
          /* kind */ context.kinds->get("OtherSource"),
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ three,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{three},
          /* features */ FeatureMayAlwaysSet{feature_one, feature_two},
          /* local_positions */ {}),
  };
  EXPECT_EQ(
      taint.propagate(
          /* caller */ one,
          /* callee */ four,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 2)),
          /* call_position */ context.positions->get("Test.java", 1),
          /* maximum_source_sink_distance */ 100,
          /* extra_features */ FeatureMayAlwaysSet{feature_three}),
      (Taint{
          Frame(
              /* kind */ context.kinds->get("TestSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 2)),
              /* callee */ four,
              /* call_position */ context.positions->get("Test.java", 1),
              /* distance */ 1,
              /* origins */ MethodSet{one},
              /* features */ FeatureMayAlwaysSet{feature_three},
              /* local_positions */ {}),
          Frame(
              /* kind */ context.kinds->get("OtherSource"),
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 2)),
              /* callee */ four,
              /* call_position */ context.positions->get("Test.java", 1),
              /* distance */ 2,
              /* origins */ MethodSet{two, three},
              /* features */
              FeatureMayAlwaysSet(
                  /* may */ FeatureSet{feature_two},
                  /* always */ FeatureSet{feature_one, feature_three}),
              /* local_positions */ {}),
      }));
}

} // namespace marianatrench

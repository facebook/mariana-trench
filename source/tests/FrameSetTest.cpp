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

  frames.add(Frame(
      /* kind */ source_kind,
      /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
      /* callee */ nullptr,
      /* call_position */ nullptr,
      /* distance */ 0,
      /* origins */ MethodSet{one},
      /* inferred_features */ FeatureMayAlwaysSet{feature_one},
      /* locally_inferred_features */ {},
      /* user_features */ {},
      /* via_type_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {}));
  EXPECT_FALSE(frames.is_bottom());
  EXPECT_EQ(frames.kind(), source_kind);
  EXPECT_EQ(
      frames,
      FrameSet{Frame(
          /* kind */ source_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ MethodSet{one},
          /* inferred_features */ FeatureMayAlwaysSet{feature_one},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})});

  frames.add(Frame(
      /* kind */ source_kind,
      /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
      /* callee */ nullptr,
      /* call_position */ nullptr,
      /* distance */ 0,
      /* origins */ MethodSet{two},
      /* inverred_features */ FeatureMayAlwaysSet{feature_two},
      /* locally_inferred_features */ {},
      /* user_features */ FeatureSet{user_feature_one},
      /* via_type_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {}));
  EXPECT_EQ(
      frames,
      FrameSet{Frame(
          /* kind */ source_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ MethodSet{one, two},
          /* inferred_features */
          FeatureMayAlwaysSet::make_may({feature_one, feature_two}),
          /* locally_inferred_features */ {},
          /* user_features */ FeatureSet{user_feature_one},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})});

  frames.add(Frame(
      /* kind */ source_kind,
      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
      /* callee */ one,
      /* call_position */ context.positions->unknown(),
      /* distance */ 3,
      /* origins */ MethodSet{one},
      /* inferred_features */ FeatureMayAlwaysSet{feature_one},
      /* locally_inferred_features */ {},
      /* user_features */ {},
      /* via_type_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {}));
  EXPECT_EQ(
      frames,
      (FrameSet{
          Frame(
              /* kind */ source_kind,
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ MethodSet{one, two},
              /* inferred_features */
              FeatureMayAlwaysSet::make_may({feature_one, feature_two}),
              /* locally_inferred_features */ {},
              /* user_features */ FeatureSet{user_feature_one},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ source_kind,
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 3,
              /* origins */ MethodSet{one},
              /* inferred_features */ FeatureMayAlwaysSet{feature_one},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
      }));

  frames.add(Frame(
      /* kind */ source_kind,
      /* callee_port */ AccessPath(Root(Root::Kind::Return)),
      /* callee */ one,
      /* call_position */ context.positions->unknown(),
      /* distance */ 3,
      /* origins */ MethodSet{two},
      /* inferred_features */ FeatureMayAlwaysSet{feature_one, feature_two},
      /* locally_inferred_features */ {},
      /* user_features */ {},
      /* via_type_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {}));
  EXPECT_EQ(
      frames,
      (FrameSet{
          Frame(
              /* kind */ source_kind,
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ MethodSet{one, two},
              /* inferred_features */
              FeatureMayAlwaysSet::make_may({feature_one, feature_two}),
              /* locally_inferred_features */ {},
              /* user_features */ FeatureSet{user_feature_one},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ source_kind,
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 3,
              /* origins */ MethodSet{one, two},
              /* inferred_features */
              FeatureMayAlwaysSet(
                  /* may */ FeatureSet{feature_one, feature_two},
                  /* always */ FeatureSet{feature_one}),
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
      }));

  // Frames with different callee ports are not merged.
  frames.add(Frame(
      /* kind */ source_kind,
      /* callee_port */ AccessPath(Root(Root::Kind::Return), Path{field}),
      /* callee */ one,
      /* call_position */ context.positions->unknown(),
      /* distance */ 3,
      /* origins */ MethodSet{one},
      /* inferred_features */ FeatureMayAlwaysSet{feature_one},
      /* locally_inferred_features */ {},
      /* user_features */ {},
      /* via_type_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {}));
  EXPECT_EQ(
      frames,
      (FrameSet{
          Frame(
              /* kind */ source_kind,
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ MethodSet{one, two},
              /* inferred_features */
              FeatureMayAlwaysSet::make_may({feature_one, feature_two}),
              /* locally_inferred_features */ {},
              /* user_features */ FeatureSet{user_feature_one},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ source_kind,
              /* callee_port */ AccessPath(Root(Root::Kind::Return)),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 3,
              /* origins */ MethodSet{one, two},
              /* inferred_features */
              FeatureMayAlwaysSet(
                  /* may */ FeatureSet{feature_one, feature_two},
                  /* always */ FeatureSet{feature_one}),
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ source_kind,
              /* callee_port */
              AccessPath(Root(Root::Kind::Return), Path{field}),
              /* callee */ one,
              /* call_position */ context.positions->unknown(),
              /* distance */ 3,
              /* origins */ MethodSet{one},
              /* inferred_features */ FeatureMayAlwaysSet{feature_one},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
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
  frames.add(Frame(
      /* kind */ Kinds::artificial_source(),
      /* callee_port */
      AccessPath(Root(Root::Kind::Argument, 0), Path{field_one, field_two}),
      /* callee */ nullptr,
      /* call_position */ nullptr,
      /* distance */ 0,
      /* origins */ {},
      /* inferred_features */ FeatureMayAlwaysSet{feature_one},
      /* locally_inferred_features */ {},
      /* user_features */ FeatureSet{user_feature_one},
      /* via_type_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {}));
  EXPECT_EQ(
      frames,
      FrameSet{Frame(
          /* kind */ Kinds::artificial_source(),
          /* callee_port */
          AccessPath(Root(Root::Kind::Argument, 0), Path{field_one, field_two}),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */ FeatureMayAlwaysSet{feature_one},
          /* locally_inferred_features */ {},
          /* user_features */ FeatureSet{user_feature_one},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})});

  // We use the common prefix of the callee port.
  frames.add(Frame(
      /* kind */ Kinds::artificial_source(),
      /* callee_port */
      AccessPath(Root(Root::Kind::Argument, 0), Path{field_one, field_three}),
      /* callee */ nullptr,
      /* call_position */ nullptr,
      /* distance */ 0,
      /* origins */ {},
      /* inferred_features */ FeatureMayAlwaysSet{feature_two},
      /* locally_inferred_features */ {},
      /* user_features */ FeatureSet{user_feature_two},
      /* via_type_of_ports */ {},
      /* local_positions */ {},
      /* canonical_names */ {}));
  EXPECT_EQ(
      frames,
      FrameSet{Frame(
          /* kind */ Kinds::artificial_source(),
          /* callee_port */
          AccessPath(Root(Root::Kind::Argument, 0), Path{field_one}),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ {},
          /* inferred_features */
          FeatureMayAlwaysSet::make_may({feature_one, feature_two}),
          /* locally_inferred_features */ {},
          /* user_features */
          FeatureSet{user_feature_one, user_feature_two},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {})});

  // We do not merge when the port root is different.
  frames.add(Frame(
      /* kind */ Kinds::artificial_source(),
      /* callee_port */
      AccessPath(Root(Root::Kind::Argument, 1), Path{field_three}),
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
      frames,
      (FrameSet{
          Frame(
              /* kind */ Kinds::artificial_source(),
              /* callee_port */
              AccessPath(Root(Root::Kind::Argument, 0), Path{field_one}),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ {},
              /* inferred_features */
              FeatureMayAlwaysSet::make_may({feature_one, feature_two}),
              /* locally_inferred_features */ {},
              /* user_features */
              FeatureSet{user_feature_one, user_feature_two},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ Kinds::artificial_source(),
              /* callee_port */
              AccessPath(Root(Root::Kind::Argument, 1), Path{field_three}),
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
           Frame(
               /* kind */ test_kind,
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
               /* kind */ test_kind,
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
       })
          .leq(FrameSet{
              Frame(
                  /* kind */ test_kind,
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
                  /* kind */ test_kind,
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
                  /* kind */ test_kind,
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
  EXPECT_FALSE(
      (FrameSet{
           Frame(
               /* kind */ test_kind,
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
               /* kind */ test_kind,
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
               /* kind */ test_kind,
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
       })
          .leq(FrameSet{
              Frame(
                  /* kind */ test_kind,
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
                  /* kind */ test_kind,
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
  EXPECT_FALSE(
      (FrameSet{
           Frame(
               /* kind */ test_kind,
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
               /* kind */ test_kind,
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
       })
          .leq(FrameSet{
              Frame(
                  /* kind */ test_kind,
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
                  /* kind */ test_kind,
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
                  /* kind */ test_kind,
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
  EXPECT_TRUE(
      (FrameSet{
           Frame(
               /* kind */ test_kind,
               /* callee_port */
               AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
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
               /* kind */ test_kind,
               /* callee_port */
               AccessPath(Root(Root::Kind::Argument, 0), Path{y}),
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
       })
          .leq(FrameSet{
              Frame(
                  /* kind */ test_kind,
                  /* callee_port */
                  AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
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
                  /* kind */ test_kind,
                  /* callee_port */
                  AccessPath(Root(Root::Kind::Argument, 0), Path{y}),
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
                  /* kind */ test_kind,
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
  EXPECT_TRUE(
      (FrameSet{
           Frame(
               /* kind */ test_kind,
               /* callee_port */
               AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}),
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
               /* kind */ test_kind,
               /* callee_port */
               AccessPath(Root(Root::Kind::Argument, 0), Path{y, x}),
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
               /* kind */ test_kind,
               /* callee_port */
               AccessPath(Root(Root::Kind::Argument, 1)),
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
       })
          .leq(FrameSet{
              Frame(
                  /* kind */ test_kind,
                  /* callee_port */
                  AccessPath(Root(Root::Kind::Argument, 0), Path{x, y}),
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
                  /* kind */ test_kind,
                  /* callee_port */
                  AccessPath(Root(Root::Kind::Argument, 0), Path{y, x}),
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
                  /* kind */ test_kind,
                  /* callee_port */ AccessPath(Root(Root::Kind::Argument, 1)),
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
      Frame(
          /* kind */ test_kind,
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
  });
  EXPECT_TRUE(frames.is_bottom());

  initial_frames = FrameSet{
      Frame(
          /* kind */ test_kind,
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
  };

  frames = initial_frames;
  frames.difference_with(FrameSet{});
  EXPECT_EQ(frames, initial_frames);

  frames = initial_frames;
  frames.difference_with(FrameSet{
      Frame(
          /* kind */ test_kind,
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
  });
  EXPECT_TRUE(frames.is_bottom());

  // Left hand side is bigger than right hand side.
  frames = initial_frames;
  frames.difference_with(FrameSet{
      Frame(
          /* kind */ test_kind,
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
  });
  EXPECT_EQ(frames, initial_frames);

  // Left hand side and right hand side have different inferred features.
  frames = initial_frames;
  frames.difference_with(FrameSet{
      Frame(
          /* kind */ test_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ one,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* inferred_features */ FeatureMayAlwaysSet{feature_two},
          /* locally_inferred_features */ {},
          /* user_features */ FeatureSet{user_feature_one},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
  });
  EXPECT_EQ(frames, initial_frames);

  // Left hand side and right hand side have different user features.
  frames = initial_frames;
  frames.difference_with(FrameSet{
      Frame(
          /* kind */ test_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ one,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{one},
          /* inferred_features */ FeatureMayAlwaysSet{feature_one},
          /* locally_inferred_features */ {},
          /* user_features */ FeatureSet{user_feature_two},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
  });
  EXPECT_EQ(frames, initial_frames);

  // Left hand side and right hand side have different callee_ports.
  frames = initial_frames;
  frames.difference_with(FrameSet{
      Frame(
          /* kind */ test_kind,
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
  });
  EXPECT_EQ(frames, initial_frames);

  frames = FrameSet{
      Frame(
          /* kind */ test_kind,
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
          /* kind */ test_kind,
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
  frames.difference_with(FrameSet{
      Frame(
          /* kind */ test_kind,
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
          /* kind */ test_kind,
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
          /* kind */ test_kind,
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
  EXPECT_TRUE(frames.is_bottom());

  frames = FrameSet{
      Frame(
          /* kind */ test_kind,
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
          /* kind */ test_kind,
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
          /* kind */ test_kind,
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
  frames.difference_with(FrameSet{
      Frame(
          /* kind */ test_kind,
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
          /* kind */ test_kind,
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
      frames,
      (FrameSet{
          Frame(
              /* kind */ test_kind,
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

  frames = FrameSet{
      Frame(
          /* kind */ test_kind,
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
          /* kind */ test_kind,
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
  frames.difference_with(FrameSet{
      Frame(
          /* kind */ test_kind,
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
          /* kind */ test_kind,
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
          /* kind */ test_kind,
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
      frames,
      (FrameSet{
          Frame(
              /* kind */ test_kind,
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
      }));

  frames = FrameSet{
      Frame(
          /* kind */ test_kind,
          /* callee_port */
          AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
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
          /* kind */ test_kind,
          /* callee_port */
          AccessPath(Root(Root::Kind::Argument, 0), Path{y}),
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
  frames.difference_with(FrameSet{
      Frame(
          /* kind */ test_kind,
          /* callee_port */
          AccessPath(Root(Root::Kind::Argument, 0), Path{x}),
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
          /* kind */ test_kind,
          /* callee_port */
          AccessPath(Root(Root::Kind::Argument, 0), Path{y}),
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
          /* kind */ test_kind,
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
  EXPECT_TRUE(frames.is_bottom());

  frames = FrameSet{
      Frame(
          /* kind */ test_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ one,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{one, two},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ test_kind,
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
          /* kind */ test_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ three,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{one, three},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
  };
  frames.difference_with(FrameSet{
      Frame(
          /* kind */ test_kind,
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
          /* kind */ test_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
          /* callee */ three,
          /* call_position */ test_position,
          /* distance */ 1,
          /* origins */ MethodSet{one, two, three},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
  });
  EXPECT_EQ(
      frames,
      (FrameSet{
          Frame(
              /* kind */ test_kind,
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
              /* callee */ one,
              /* call_position */ test_position,
              /* distance */ 1,
              /* origins */ MethodSet{one, two},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ test_kind,
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
      Frame(
          /* kind */ test_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ MethodSet{one},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ test_kind,
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
          /* kind */ test_kind,
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
  frames.map([feature_one](Frame& frame) {
    frame.add_inferred_features(FeatureMayAlwaysSet{feature_one});
  });
  EXPECT_EQ(
      frames,
      (FrameSet{
          Frame(
              /* kind */ test_kind,
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ MethodSet{one},
              /* inferred_features */ {},
              /* locally_inferred_features */ FeatureMayAlwaysSet{feature_one},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ test_kind,
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
              /* callee */ two,
              /* call_position */ test_position,
              /* distance */ 1,
              /* origins */ MethodSet{two},
              /* inferred_features */ {},
              /* locally_inferred_features */ FeatureMayAlwaysSet{feature_one},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ test_kind,
              /* callee_port */ AccessPath(Root(Root::Kind::Argument, 0)),
              /* callee */ three,
              /* call_position */ test_position,
              /* distance */ 1,
              /* origins */ MethodSet{three},
              /* inferred_features */ {},
              /* locally_inferred_features */ FeatureMayAlwaysSet{feature_one},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
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
      Frame(
          /* kind */ test_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ MethodSet{one},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ test_kind,
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
          /* kind */ test_kind,
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
  frames.filter(
      [](const Frame& frame) { return frame.callee_port().root().is_leaf(); });
  EXPECT_EQ(
      frames,
      (FrameSet{
          Frame(
              /* kind */ test_kind,
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ MethodSet{one},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
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
      Frame(
          /* kind */ test_kind,
          /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
          /* callee */ nullptr,
          /* call_position */ nullptr,
          /* distance */ 0,
          /* origins */ MethodSet{one},
          /* inferred_features */ {},
          /* locally_inferred_features */ {},
          /* user_features */ {},
          /* via_type_of_ports */ {},
          /* local_positions */ {},
          /* canonical_names */ {}),
      Frame(
          /* kind */ test_kind,
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
          /* canonical_names */ {})};

  auto new_kind = context.kinds->get("TestSink2");
  auto frame_with_new_kind = frames.with_kind(new_kind);

  EXPECT_EQ(frame_with_new_kind.kind(), new_kind);
  EXPECT_EQ(
      frame_with_new_kind,
      (FrameSet{
          Frame(
              /* kind */ new_kind,
              /* callee_port */ AccessPath(Root(Root::Kind::Leaf)),
              /* callee */ nullptr,
              /* call_position */ nullptr,
              /* distance */ 0,
              /* origins */ MethodSet{one},
              /* inferred_features */ {},
              /* locally_inferred_features */ {},
              /* user_features */ {},
              /* via_type_of_ports */ {},
              /* local_positions */ {},
              /* canonical_names */ {}),
          Frame(
              /* kind */ new_kind,
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
              /* canonical_names */ {})}));
}

} // namespace marianatrench

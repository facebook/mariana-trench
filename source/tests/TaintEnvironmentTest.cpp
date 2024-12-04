/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gtest/gtest.h>

#include <IRInstruction.h>

#include <mariana-trench/Redex.h>
#include <mariana-trench/TaintEnvironment.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class TaintEnvironmentTest : public test::Test {};

TEST_F(TaintEnvironmentTest, ReadAndWritePointsToTreeSimple) {
  // Setup instructions to create memory locations
  auto i0 = std::make_unique<IRInstruction>(OPCODE_CONST);
  auto i1 = std::make_unique<IRInstruction>(OPCODE_CONST_CLASS);
  auto i2 = std::make_unique<IRInstruction>(OPCODE_RETURN_VOID);

  // Setup memory locations
  auto r0 = std::make_unique<ParameterMemoryLocation>(0);
  auto im0 = std::make_unique<InstructionMemoryLocation>(i0.get());
  auto im1 = std::make_unique<InstructionMemoryLocation>(i1.get());
  auto im2 = std::make_unique<InstructionMemoryLocation>(i2.get());

  // Setup fields
  const auto x = PathElement::field("x");
  const auto y = PathElement::field("y");

  //
  // Tests for field assignments to instruction memory locations.
  //
  auto environment = TaintEnvironment::bottom();

  // Test strong write to field of a root memory location.
  // eg. r0.x = im0();
  auto im0_set = PointsToSet{im0.get()};
  environment.write(
      /* memory_location */ r0.get(),
      /* field */ x.name(),
      /* points_tos */ im0_set,
      UpdateKind::Strong);
  auto r0_x = r0.get()->make_field(x.name());
  EXPECT_EQ(environment.points_to(r0_x), im0_set);

  // Test weak write to existing path.
  // eg. join with r0.x = im1();
  auto im1_set = PointsToSet{im1.get()};
  environment.write(
      /* memory_location */ r0.get(),
      /* field */ x.name(),
      /* points_tos */ im1_set,
      UpdateKind::Weak);
  auto im0_im1_set = PointsToSet{im0.get(), im1.get()};
  EXPECT_EQ(environment.points_to(r0_x), im0_im1_set);

  // Test write to field memory location
  // eg. r0.x.y = im2();
  // Here, r0.x = FieldMemoryLocation(MemoryLocation(r0), x) resolves to {im0,
  // im1}
  // => results in a writes to root memory locations:
  // im0.y = im2
  // im1.y = im2
  auto im2_set = PointsToSet{im2.get()};
  environment.write(
      /* memory_location */ r0_x,
      /* field */ y.name(),
      /* points_tos */ im2_set,
      UpdateKind::Strong);
  auto r0_x_y = r0_x->make_field(y.name());
  EXPECT_EQ(environment.points_to(r0_x), im0_im1_set);
  EXPECT_EQ(environment.points_to(r0_x_y), im2_set);
  EXPECT_EQ(
      environment.get(im0.get()).aliases().raw_read(Path{y}),
      PointsToTree{im2_set});
  EXPECT_EQ(
      environment.get(im1.get()).aliases().raw_read(Path{y}),
      PointsToTree{im2_set});
  EXPECT_EQ(
      environment.resolve_aliases(r0.get()),
      (PointsToTree{
          {Path{}, PointsToSet{r0.get()}},
          {Path{x}, im0_im1_set},
          {Path{x, y}, im2_set},
      }));

  // Test current state of the taint environment
  EXPECT_EQ(
      environment,
      (TaintEnvironment{
          {r0.get(),
           PointsToTree{
               {Path{x}, im0_im1_set},
           }},
          {im0.get(),
           PointsToTree{
               {Path{y}, im2_set},
           }},
          {im1.get(),
           PointsToTree{
               {Path{y}, im2_set},
           }}}));

  // Test strong write to break existing aliases.
  // eg. r0.x = im0_im1();
  //     r0.x = im2();
  environment.write(
      /* memory_location */ r0.get(),
      /* field */ x.name(),
      /* points_tos */ im2_set,
      UpdateKind::Strong);
  EXPECT_EQ(environment.points_to(r0_x), im2_set);
  EXPECT_TRUE(environment.points_to(r0_x_y).is_bottom());
  EXPECT_EQ(
      environment.resolve_aliases(r0.get()),
      (PointsToTree{
          {Path{}, PointsToSet{r0.get()}},
          {Path{x}, im2_set},
      }));

  // Test current state of the taint environment
  EXPECT_EQ(
      environment,
      (TaintEnvironment{
          {r0.get(),
           PointsToTree{
               {Path{x}, im2_set},
           }},
          {im0.get(),
           PointsToTree{
               {Path{y}, im2_set},
           }},
          {im1.get(),
           PointsToTree{
               {Path{y}, im2_set},
           }}}));
}

TEST_F(TaintEnvironmentTest, ChainingPointsToTree) {
  // Setup instructions to create memory locations
  auto i0 = std::make_unique<IRInstruction>(OPCODE_CONST);
  auto i1 = std::make_unique<IRInstruction>(OPCODE_CONST_CLASS);
  auto i2 = std::make_unique<IRInstruction>(OPCODE_RETURN_VOID);
  auto i3 = std::make_unique<IRInstruction>(OPCODE_RETURN_OBJECT);

  // Setup memory locations
  auto r0 = std::make_unique<ParameterMemoryLocation>(0);
  auto im0 = std::make_unique<InstructionMemoryLocation>(i0.get());
  auto im1 = std::make_unique<InstructionMemoryLocation>(i1.get());
  auto im2 = std::make_unique<InstructionMemoryLocation>(i2.get());
  auto im3 = std::make_unique<InstructionMemoryLocation>(i3.get());

  // Setup fields
  const auto x = PathElement::field("x");

  // Setup points-to sets
  auto im0_set = PointsToSet{im0.get()};
  auto r0_x = r0.get()->make_field(x.name());
  auto im1_set = PointsToSet{im1.get()};
  auto im0_im1_set = PointsToSet{im0.get(), im1.get()};
  auto im2_set = PointsToSet{im2.get()};

  //
  // Tests for field assignments to other memory locations with existing
  // points-to trees (chaining aliases in the taint environment)
  //
  auto environment = TaintEnvironment::bottom();
  auto r1 = std::make_unique<ParameterMemoryLocation>(1);
  const auto a = PathElement::field("a");
  const auto b = PathElement::field("b");

  // Test strong assignment of a variable to a field
  // eg. r0.x = r1;
  auto r1_set = PointsToSet{r1.get()};
  environment.write(
      /* memory_location */ r0.get(),
      /* field */ x.name(),
      /* points_tos */ r1_set,
      UpdateKind::Strong);
  EXPECT_EQ(environment.points_to(r0_x), r1_set);

  // Test update from the aliased memory location
  // eg. r1.a = im1();
  environment.write(
      /* memory_location */ r1.get(),
      /* field */ a.name(),
      /* points_tos */ im1_set,
      UpdateKind::Strong);

  // Test read from aliased memory location i.e. r1.a
  auto r1_a = r1->make_field(a.name());
  EXPECT_EQ(environment.points_to(r1_a), im1_set);
  // Test read from alias i.e. r0.x.a
  auto r0_x_a = r0_x->make_field(a.name());
  EXPECT_EQ(environment.points_to(r0_x_a), im1_set);

  // Test update from the aliasing memory location
  // eg. r0.x.a.b = im2();
  environment.write(
      /* memory_location */ r0_x_a,
      /* field */ b.name(),
      /* points_tos */ im2_set,
      UpdateKind::Strong);

  // Test read from alias i.e. r0.x.a.b
  auto r0_x_a_b = r0_x_a->make_field(b.name());
  EXPECT_EQ(environment.points_to(r0_x_a_b), im2_set);
  // Test read from alias i.e. r1.a.b
  auto r1_a_b = r1_a->make_field(b.name());
  EXPECT_EQ(environment.points_to(r1_a_b), im2_set);

  // Test current state of the taint environment
  EXPECT_EQ(
      environment,
      (TaintEnvironment{
          {r0.get(), PointsToTree{{Path{x}, r1_set}}},
          {r1.get(), PointsToTree{{Path{a}, im1_set}}},
          {im1.get(), PointsToTree{{Path{b}, im2_set}}},
      }));

  //
  // Tests with deep alias chains in the taint environment
  //
  auto r2 = std::make_unique<ParameterMemoryLocation>(2);
  const auto c = PathElement::field("c");
  const auto d = PathElement::field("d");

  // Test setup new root memory location
  // eg. r2.c = im0()
  environment.write(
      /* memory_location */ r2.get(),
      /* field */ c.name(),
      /* points_tos */ im0_set,
      UpdateKind::Strong);
  auto r2_c = r2->make_field(c.name());
  EXPECT_EQ(environment.points_to(r2_c), im0_set);

  // eg. r2.d = im3();
  auto im3_set = PointsToSet{im3.get()};
  environment.write(
      /* memory_location */ r2.get(),
      /* field */ d.name(),
      /* points_tos */ im3_set,
      UpdateKind::Strong);
  auto r2_d = r2->make_field(d.name());
  EXPECT_EQ(environment.points_to(r2_d), im3_set);

  // Setup to test the current state of the environment
  // r0 => .x -> {r1}
  auto r0_tree = PointsToTree{
      {Path{x}, PointsToSet{r1.get()}},
  };
  // r1 => .a -> {im1}
  auto r1_tree = PointsToTree{
      {Path{a}, PointsToSet{im1.get()}},
  };
  // r2 => .c -> {im0}
  //       .d -> {im3}
  auto r2_tree = PointsToTree{
      {Path{c}, PointsToSet{im0.get()}},
      {Path{d}, PointsToSet{im3.get()}},
  };
  // im1 => .b -> {im2}
  auto im1_tree = PointsToTree{
      {Path{b}, PointsToSet{im2.get()}},
  };
  auto expected = TaintEnvironment{
      {r0.get(), r0_tree},
      {r1.get(), r1_tree},
      {r2.get(), r2_tree},
      {im1.get(), im1_tree},
  };

  //
  // Test the current taint environment
  //
  EXPECT_EQ(environment, expected);
  EXPECT_EQ(
      environment.resolve_aliases(r0.get()),
      (PointsToTree{
          {Path{}, PointsToSet{r0.get()}},
          {Path{x}, PointsToSet{r1.get()}},
          {Path{x, a}, im1_set},
          {Path{x, a, b}, im2_set}}));
  EXPECT_EQ(
      environment.resolve_aliases(r1.get()),
      (PointsToTree{
          {Path{}, PointsToSet{r1.get()}},
          {Path{a}, im1_set},
          {Path{a, b}, im2_set}}));
  EXPECT_EQ(
      environment.resolve_aliases(r2.get()),
      (PointsToTree{
          {Path{}, PointsToSet{r2.get()}},
          {Path{c}, im0_set},
          {Path{d}, im3_set}}));
  EXPECT_EQ(
      environment.resolve_aliases(im1.get()),
      (PointsToTree{{Path{}, PointsToSet{im1.get()}}, {Path{b}, im2_set}}));
}

TEST_F(TaintEnvironmentTest, OverlappingWithEmptyIntermediateNodePointsToTree) {
  // Setup instructions to create memory locations
  auto i0 = std::make_unique<IRInstruction>(OPCODE_CONST);
  auto i1 = std::make_unique<IRInstruction>(OPCODE_CONST_CLASS);
  auto i2 = std::make_unique<IRInstruction>(OPCODE_RETURN_VOID);
  auto i3 = std::make_unique<IRInstruction>(OPCODE_RETURN_OBJECT);

  // Setup memory locations
  auto r0 = std::make_unique<ParameterMemoryLocation>(0);
  auto r1 = std::make_unique<ParameterMemoryLocation>(1);
  auto r2 = std::make_unique<ParameterMemoryLocation>(2);
  auto im0 = std::make_unique<InstructionMemoryLocation>(i0.get());
  auto im1 = std::make_unique<InstructionMemoryLocation>(i1.get());
  auto im2 = std::make_unique<InstructionMemoryLocation>(i2.get());
  auto im3 = std::make_unique<InstructionMemoryLocation>(i3.get());
  auto im3_set = PointsToSet{im3.get()};

  // Setup fields
  const auto x = PathElement::field("x");
  const auto a = PathElement::field("a");
  const auto b = PathElement::field("b");
  const auto c = PathElement::field("c");
  const auto d = PathElement::field("d");

  // Setup points-to sets
  auto r1_set = PointsToSet{r1.get()};
  auto im0_set = PointsToSet{im0.get()};
  auto im1_set = PointsToSet{im1.get()};
  auto im0_im1_set = PointsToSet{im0.get(), im1.get()};
  auto im2_set = PointsToSet{im2.get()};

  // Setup to test the current state of the environment
  auto environment = TaintEnvironment::bottom();
  environment.write(im1.get(), b.name(), im2_set, UpdateKind::Strong);
  environment.write(r0.get(), x.name(), r1_set, UpdateKind::Strong);
  environment.write(r1.get(), a.name(), im1_set, UpdateKind::Strong);
  environment.write(r2.get(), c.name(), im0_set, UpdateKind::Strong);
  environment.write(r2.get(), d.name(), im3_set, UpdateKind::Strong);

  EXPECT_EQ(
      environment,
      (TaintEnvironment{
          {r0.get(), PointsToTree{{Path{x}, PointsToSet{r1.get()}}}},
          {r1.get(), PointsToTree{{Path{a}, PointsToSet{im1.get()}}}},
          {r2.get(),
           PointsToTree{{Path{c}, PointsToSet{im0.get()}}, {Path{d}, im3_set}}},
          {im1.get(), PointsToTree{{Path{b}, PointsToSet{im2.get()}}}},
      }));

  //
  // Test for:
  // - Assigned value is a field memory location (r0.x.a.b) which exists in
  // the points-to tree.
  // - Test deep points-to tree: the path written to has intermediate nodes
  // that doesn't exist in points-to tree. i.e. FieldMemoryLocation is created
  // but does not resolved to anything on the points-to tree for the root
  // memory location.
  //
  // eg. r1.b.c.d = r2.d
  // Here, r2.d -> {im3}
  //       r1.b -> _|_
  auto r2_d = r2->make_field(d.name());
  auto r1_b_c = r1->make_field(Path{b, c});
  auto r2_d_points_to = environment.points_to(r2_d);
  environment.write(
      /* memory_location */ r1_b_c,
      /* field */ d.name(),
      /* points_tos */ r2_d_points_to,
      UpdateKind::Strong);
  auto r1_b_c_d = r1->make_field(Path{b, c, d});
  // Test read from r1.b.c.d and r0.x.a.b points-to the same memory locations.
  EXPECT_EQ(environment.points_to(r1_b_c_d), environment.points_to(r2_d));
  // Test read from r1.b.c.d is the resolved memory location {im2}
  EXPECT_EQ(environment.points_to(r1_b_c_d), im3_set);
  // Test resolved aliases for r1.
  EXPECT_EQ(
      environment.resolve_aliases(r1.get()),
      (PointsToTree{
          {Path{}, PointsToSet{r1.get()}},
          {Path{a}, im1_set},
          {Path{a, b}, im2_set},
          {Path{b, c, d}, im3_set}}));

  // Test write to the memory location aliased by multiple locations.
  // eg. im3.z = r3();
  const auto z = PathElement::field("z");
  auto r3 = std::make_unique<ParameterMemoryLocation>(3);
  auto r3_set = PointsToSet{r3.get()};
  environment.write(
      /* memory_location */ im3.get(),
      /* field */ z.name(),
      /* points_tos */ r3_set,
      UpdateKind::Strong);

  // Test read from r1.b.c.d.z
  auto r1_b_c_d_z = r1_b_c_d->make_field(z.name());
  EXPECT_EQ(environment.points_to(r1_b_c_d_z), r3_set);
  // Test read from r2.d.z
  auto r2_d_z = r2->make_field(Path{d, z});
  EXPECT_EQ(environment.points_to(r2_d_z), r3_set);

  // Update setup to test the current state of the environment
  auto r0_tree = PointsToTree{
      {Path{x}, PointsToSet{r1.get()}},
  };
  // r1 => .a -> {im1}
  //       .b.c.d -> {im3}
  auto r1_tree = PointsToTree{
      {Path{a}, PointsToSet{im1.get()}},
      {Path{b, c, d}, PointsToSet{im3.get()}},
  };
  // r2 => .c -> {im0}
  //       .d -> {im3}
  auto r2_tree = PointsToTree{
      {Path{c}, PointsToSet{im0.get()}},
      {Path{d}, PointsToSet{im3.get()}},
  };
  // im1 => .b -> {im2}
  auto im1_tree = PointsToTree{
      {Path{b}, PointsToSet{im2.get()}},
  };
  // im3 => .z -> {r3}
  auto im3_tree = PointsToTree{
      {Path{z}, r3_set},
  };
  auto expected = TaintEnvironment{
      {r0.get(), r0_tree},
      {r1.get(), r1_tree},
      {r2.get(), r2_tree},
      {im1.get(), im1_tree},
      {im3.get(), im3_tree},
  };
  EXPECT_EQ(environment, expected);

  // Test weak update at the middle of an existing path in the points-to
  // tree. eg.
  // if () { r1.b.c.d.z = r2(); } // i.e. existing state.
  // else { r1.b = r4(); }
  auto r4 = std::make_unique<ParameterMemoryLocation>(4);
  auto r4_set = PointsToSet{r4.get()};
  environment.write(
      /* memory_location */ r1.get(),
      /* field */ b.name(),
      /* points_tos */ r4_set,
      UpdateKind::Weak);
  auto r1_b = r1->make_field(b.name());
  // Test read from r1.b
  EXPECT_EQ(environment.points_to(r1_b), r4_set);

  //
  // Test update to taint environment so that multiple points-to trees
  // need to be merged when updating common path in different subtrees.
  //

  auto i4 = std::make_unique<IRInstruction>(IOPCODE_MOVE_RESULT_PSEUDO_OBJECT);
  auto im4 = std::make_unique<InstructionMemoryLocation>(i4.get());
  auto im4_set = PointsToSet{im4.get()};

  // Here, r1  => .a     -> {im1}
  //              .b     -> {r4}
  //              .b.c.d -> {im3}
  // Setup r4.c = im4();
  // Now, im4 is also reachable through r1.b.c when we resolve aliases.
  environment.write(
      /* memory_location */ r4.get(),
      /* field */ c.name(),
      /* points_tos */ im4_set,
      UpdateKind::Weak);
  auto r4_c = r4->make_field(c.name());
  EXPECT_EQ(environment.points_to(r4_c), im4_set);
  EXPECT_EQ(environment.points_to(r1_b_c_d), im3_set);
  // r1.b.c is still bottom when we read the points-to as it does not
  // directly alias it. From taint perspective, a deep read will
  // resolved and merge in the taint tree at r4.c.
  EXPECT_TRUE(environment.points_to(r1_b_c).is_bottom());

  // Test update to path that is bottom in the current root but is
  // resolved to a different memory location in another tree. e.g r1.b.c
  // = im4() Here, although r1.b.c exists in r1's points-to tree, it is
  // bottom. Here, we can resolve to r4.c through the alias at r1.b and
  // hence the write is equivalent to r4.c = im4();, which is already
  // the current state.
  environment.write(
      /* memory_location */ r1_b,
      /* field */ c.name(),
      /* points_tos */ im4_set,
      UpdateKind::Weak);
  EXPECT_TRUE(environment.points_to(r1_b_c).is_bottom());
  EXPECT_EQ(environment.points_to(r1_b_c_d), im3_set);
  EXPECT_EQ(environment.points_to(r4_c), im4_set);

  // Setup r4 to have paths parallel to existing paths i.e. r4.c.d,
  // which is reachable through r1.b.c.d eg. r4.c.d = im5();
  auto i5 = std::make_unique<IRInstruction>(OPCODE_IGET);
  auto im5 = std::make_unique<InstructionMemoryLocation>(i5.get());
  auto im5_set = PointsToSet{im5.get()};
  environment.write(
      /* memory_location */ r4_c,
      /* field */ d.name(),
      /* points_tos */ im5_set,
      UpdateKind::Weak);
  auto r4_c_d = r4_c->make_field(d.name());
  EXPECT_EQ(environment.points_to(r4_c_d), im5_set);
  // r1.b.c.d -> {im3} still.
  EXPECT_EQ(environment.points_to(r1_b_c_d), im3_set);

  // Here, r1  => .a     -> {im1}
  //              .b     -> {r4}
  //              .b.c.d -> {im3}
  //       r4  => .c     -> {im4}
  //       im4 => .d     -> {im5}
  //
  // Now r1.b.c.d can resolve to 2 different memory locations via:
  // - r1.b.c.d = {im3}
  // - r1.b -> {r4}, r4.c -> {im4}, im4.d -> {im5}
  // eg. r1.b.c.d.e = im0();
  // implies writes to both im3.e and im5.e
  auto e = PathElement::field("e");
  environment.write(
      /* memory_location */ r1_b_c_d,
      /* field */ e.name(),
      /* points_tos */ im0_set,
      UpdateKind::Weak);
  auto r1_b_c_d_e = r1_b_c_d->make_field(e.name());
  EXPECT_EQ(environment.points_to(r1_b_c_d_e), im0_set);
  auto r4_c_d_e = r4_c_d->make_field(e.name());
  EXPECT_EQ(environment.points_to(r4_c_d_e), im0_set);
  auto im3_e = im3->make_field(e.name());
  EXPECT_EQ(environment.points_to(im3_e), im0_set);
  auto im5_e = im5->make_field(e.name());
  EXPECT_EQ(environment.points_to(im5_e), im0_set);

  // Update setup to test the current state of the environment
  // r1 => .a     -> {im1}
  //       .b     -> {r4}
  //       .b.c.d -> {im3}
  r1_tree = PointsToTree{
      {Path{a}, PointsToSet{im1.get()}},
      {Path{b}, PointsToSet{r4.get()}},
      {Path{b, c, d}, PointsToSet{im3.get()}},
  };
  // r4 => .c     -> {im4}
  auto r4_tree = PointsToTree{
      {Path{c}, im4_set},
  };
  // im3 => .e     -> {im0}
  //        .z     -> {r3}
  im3_tree = PointsToTree{
      {Path{e}, im0_set},
      {Path{z}, r3_set},
  };
  // im4 => .d -> {im5}
  auto im4_tree = PointsToTree{
      {Path{d}, im5_set},
  };
  // im5 => .e -> {im0}
  auto im5_tree = PointsToTree{
      {Path{e}, im0_set},
  };

  // Expected: TaintEnvironment(
  //   r0(ParameterMemoryLocation(0)) =>
  //     `.x` -> r1(ParameterMemoryLocation(1))
  //
  //   r1(ParameterMemoryLocation(1)) =>
  //     `.a` -> im1(`CONST_CLASS)
  //     `.b` -> r4(ParameterMemoryLocation(4))
  //         `.c` ->
  //             `.d` -> im3(RETURN_OBJECT)
  //
  //   r2(ParameterMemoryLocation(2)) =>
  //     `.c` -> im0(CONST)
  //     `.d` -> im3(RETURN_OBJECT)
  //
  //   r4(ParameterMemoryLocation(4)) =>
  //     `.c` -> im4(IOPCODE_MOVE_RESULT_PSEUDO_OBJECT)
  //
  //   im1(CONST_CLASS) =>
  //     `.b` -> im2(RETURN_VOID)
  //
  //   im3(RETURN_OBJECT) =>
  //     `.e` -> im0(CONST)
  //     `.z` -> r3(ParameterMemoryLocation(3))
  //
  //   im4(IOPCODE_MOVE_RESULT_PSEUDO_OBJECT) =>
  //     `.d` -> im5(IGET)
  //
  //   im5(IGET) =>
  //     `.e` -> im0(CONST)
  expected = TaintEnvironment{
      {r0.get(), r0_tree},
      {r1.get(), r1_tree},
      {r2.get(), r2_tree},
      {r4.get(), r4_tree},
      {im1.get(), im1_tree},
      {im3.get(), im3_tree},
      {im4.get(), im4_tree},
      {im5.get(), im5_tree},
  };
  EXPECT_EQ(environment, expected);

  //
  // Test resolved aliases
  //
  EXPECT_EQ(
      environment.resolve_aliases(r0.get()),
      (PointsToTree{
          {Path{}, PointsToSet{r0.get()}},
          {Path{x}, PointsToSet{r1.get()}},
          {Path{x, a}, im1_set},
          {Path{x, a, b}, im2_set},
          {Path{x, b}, r4_set},
          {Path{x, b, c}, im4_set},
          {Path{x, b, c, d}, im3_set.join(im5_set)},
          {Path{x, b, c, d, e}, im0_set},
          {Path{x, b, c, d, z}, r3_set}}));

  EXPECT_EQ(
      environment.resolve_aliases(r1.get()),
      (PointsToTree{
          {Path{}, PointsToSet{r1.get()}},
          {Path{a}, im1_set},
          {Path{a, b}, im2_set},
          {Path{b}, r4_set},
          {Path{b, c}, im4_set},
          {Path{b, c, d}, im3_set.join(im5_set)},
          {Path{b, c, d, e}, im0_set},
          {Path{b, c, d, z}, r3_set}}));

  EXPECT_EQ(
      environment.resolve_aliases(r2.get()),
      (PointsToTree{
          {Path{}, PointsToSet{r2.get()}},
          {Path{c}, im0_set},
          {Path{d}, im3_set},
          // im0_set is already in the tree but on a different branch.
          // Hence, this is not a cycle.
          {Path{d, e}, im0_set},
          {Path{d, z}, r3_set}}));

  EXPECT_EQ(
      environment.resolve_aliases(r4.get()),
      (PointsToTree{
          {Path{}, PointsToSet{r4.get()}},
          {Path{c}, im4_set},
          {Path{c, d}, im5_set},
          {Path{c, d, e}, im0_set}}));

  EXPECT_EQ(
      environment.resolve_aliases(im1.get()),
      (PointsToTree{{Path{}, PointsToSet{im1.get()}}, {Path{b}, im2_set}}));

  EXPECT_EQ(
      environment.resolve_aliases(im3.get()),
      (PointsToTree{
          {Path{}, PointsToSet{im3.get()}},
          {Path{e}, im0_set},
          {Path{z}, r3_set}}));

  EXPECT_EQ(
      environment.resolve_aliases(im4.get()),
      (PointsToTree{
          {Path{}, PointsToSet{im4.get()}},
          {Path{d}, im5_set},
          {Path{d, e}, im0_set}}));

  EXPECT_EQ(
      environment.resolve_aliases(im5.get()),
      (PointsToTree{{Path{}, PointsToSet{im5.get()}}, {Path{e}, im0_set}}));
}

} // namespace marianatrench

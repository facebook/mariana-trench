/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <gmock/gmock.h>

#include <mariana-trench/TransformList.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {
class TransformListTest : public test::Test {};

TEST_F(TransformListTest, Canonicalize) {
  auto context = test::make_empty_context();

  const Transform* transform_x =
      context.transforms_factory->create_transform("X");
  const Transform* transform_y =
      context.transforms_factory->create_transform("Y");
  const Transform* transform_z =
      context.transforms_factory->create_transform("Z");

  const Kind* kind_a = context.kind_factory->get("A");
  const Kind* kind_b = context.kind_factory->get("B");
  const Transform* sanitize_a =
      context.transforms_factory->create_sanitizer_set_transform(
          SanitizerSetTransform::Set{kind_a});
  const Transform* sanitize_b =
      context.transforms_factory->create_sanitizer_set_transform(
          SanitizerSetTransform::Set{kind_b});
  const Transform* sanitize_a_b =
      context.transforms_factory->create_sanitizer_set_transform(
          SanitizerSetTransform::Set{kind_a, kind_b});

  // canonicalize does not change the list if there is no sanitizer
  EXPECT_EQ(
      TransformList::canonicalize(
          context.transforms_factory->create(
              std::vector{transform_x, transform_y, transform_z}),
          *context.transforms_factory),
      *context.transforms_factory->create(
          std::vector{transform_x, transform_y, transform_z}));

  // No duplication, sorted
  EXPECT_EQ(
      TransformList::canonicalize(
          context.transforms_factory->create(
              std::vector{sanitize_a, sanitize_b}),
          *context.transforms_factory),
      *context.transforms_factory->create(std::vector{sanitize_a_b}));

  // No duplication, unsorted
  EXPECT_EQ(
      TransformList::canonicalize(
          context.transforms_factory->create(
              std::vector{sanitize_b, sanitize_a}),
          *context.transforms_factory),
      *context.transforms_factory->create(std::vector{sanitize_a_b}));

  EXPECT_EQ(
      TransformList::canonicalize(
          context.transforms_factory->create(
              std::vector{sanitize_a_b, sanitize_a}),
          *context.transforms_factory),
      *context.transforms_factory->create(std::vector{sanitize_a_b}));

  // Duplication, sorted
  EXPECT_EQ(
      TransformList::canonicalize(
          context.transforms_factory->create(
              std::vector{sanitize_a, sanitize_a, sanitize_b, sanitize_b}),
          *context.transforms_factory),
      *context.transforms_factory->create(std::vector{sanitize_a_b}));

  // Duplication, unsorted
  EXPECT_EQ(
      TransformList::canonicalize(
          context.transforms_factory->create(
              std::vector{sanitize_b, sanitize_a, sanitize_a, sanitize_b}),
          *context.transforms_factory),
      *context.transforms_factory->create(std::vector{sanitize_a_b}));

  // Mix with transforms
  EXPECT_EQ(
      TransformList::canonicalize(
          context.transforms_factory->create(std::vector{
              transform_z, sanitize_b, sanitize_a, sanitize_a, transform_y}),
          *context.transforms_factory),
      *context.transforms_factory->create(
          std::vector{transform_z, sanitize_a_b, transform_y}));

  EXPECT_EQ(
      TransformList::canonicalize(
          context.transforms_factory->create(std::vector{
              sanitize_b,
              sanitize_b,
              transform_x,
              transform_z,
              transform_y,
              sanitize_a,
              sanitize_b,
              sanitize_a}),
          *context.transforms_factory),
      *context.transforms_factory->create(std::vector{
          sanitize_b, transform_x, transform_z, transform_y, sanitize_a_b}));
}

TEST_F(TransformListTest, Sanitize) {
  constexpr auto Forward = TransformList::ApplicationDirection::Forward;
  constexpr auto Backward = TransformList::ApplicationDirection::Backward;

  auto context = test::make_empty_context();

  const Transform* transform_x =
      context.transforms_factory->create_transform("X");
  const Transform* transform_y =
      context.transforms_factory->create_transform("Y");
  const Transform* transform_z =
      context.transforms_factory->create_transform("Z");

  const Kind* kind_a = context.kind_factory->get("A");
  const Kind* kind_b = context.kind_factory->get("B");

  const Transform* sanitize_a =
      context.transforms_factory->create_sanitizer_set_transform(
          SanitizerSetTransform::Set{kind_a});
  const Transform* sanitize_b =
      context.transforms_factory->create_sanitizer_set_transform(
          SanitizerSetTransform::Set{kind_b});
  const Transform* sanitize_a_b =
      context.transforms_factory->create_sanitizer_set_transform(
          SanitizerSetTransform::Set{kind_a, kind_b});

  // No sanitizers
  EXPECT_FALSE(context.transforms_factory
                   ->create(std::vector{transform_x, transform_y, transform_z})
                   ->sanitizes<Forward>(kind_a));
  EXPECT_FALSE(context.transforms_factory
                   ->create(std::vector{transform_x, transform_y, transform_z})
                   ->sanitizes<Backward>(kind_a));
  EXPECT_FALSE(context.transforms_factory
                   ->create(std::vector{transform_z, transform_x, transform_y})
                   ->sanitizes<Forward>(kind_a));
  EXPECT_FALSE(context.transforms_factory
                   ->create(std::vector{transform_z, transform_x, transform_y})
                   ->sanitizes<Backward>(kind_a));

  // Sanitizer in the front/back
  EXPECT_TRUE(context.transforms_factory
                  ->create(std::vector{sanitize_a, transform_x, transform_y})
                  ->sanitizes<Forward>(kind_a));
  EXPECT_TRUE(context.transforms_factory
                  ->create(std::vector{transform_x, transform_y, sanitize_a})
                  ->sanitizes<Backward>(kind_a));

  // Multiple sanitizer in the front/back
  EXPECT_TRUE(context.transforms_factory
                  ->create(std::vector{sanitize_a_b, transform_x, transform_y})
                  ->sanitizes<Forward>(kind_a));
  EXPECT_TRUE(context.transforms_factory
                  ->create(std::vector{transform_x, transform_y, sanitize_a_b})
                  ->sanitizes<Backward>(kind_b));
  EXPECT_TRUE(context.transforms_factory
                  ->create(std::vector{sanitize_a_b, transform_x, transform_y})
                  ->sanitizes<Forward>(kind_b));
  EXPECT_TRUE(context.transforms_factory
                  ->create(std::vector{transform_x, transform_y, sanitize_a_b})
                  ->sanitizes<Backward>(kind_a));

  // Sanitizer in the middle
  EXPECT_FALSE(context.transforms_factory
                   ->create(std::vector{transform_z, sanitize_a_b, transform_y})
                   ->sanitizes<Forward>(kind_a));
  EXPECT_FALSE(context.transforms_factory
                   ->create(std::vector{transform_z, sanitize_a_b, transform_y})
                   ->sanitizes<Backward>(kind_a));

  // Passing in a TransformKind
  EXPECT_TRUE(
      context.transforms_factory
          ->create(std::vector{transform_x, transform_y, sanitize_a})
          ->sanitizes<Backward>(context.kind_factory->transform_kind(
              kind_a,
              context.transforms_factory->create(std::vector{sanitize_b}),
              nullptr)));
  EXPECT_FALSE(
      context.transforms_factory
          ->create(std::vector{transform_x, transform_y, sanitize_a})
          ->sanitizes<Backward>(context.kind_factory->transform_kind(
              kind_a,
              context.transforms_factory->create(std::vector{transform_z}),
              nullptr)));
  EXPECT_FALSE(
      context.transforms_factory->create(std::vector{sanitize_a, transform_x})
          ->sanitizes<Backward>(context.kind_factory->transform_kind(
              kind_a,
              context.transforms_factory->create(std::vector{transform_z}),
              nullptr)));
  EXPECT_FALSE(context.transforms_factory
                   ->create(std::vector{transform_x, transform_y, sanitize_a})
                   ->sanitizes<Backward>(context.kind_factory->transform_kind(
                       kind_a,
                       context.transforms_factory->create(
                           std::vector{transform_z, sanitize_b}),
                       nullptr)));
  EXPECT_FALSE(context.transforms_factory
                   ->create(std::vector{transform_x, transform_y, sanitize_a})
                   ->sanitizes<Backward>(context.kind_factory->transform_kind(
                       kind_a,
                       context.transforms_factory->create(
                           std::vector{sanitize_b, transform_z}),
                       nullptr)));
  EXPECT_FALSE(context.transforms_factory
                   ->create(std::vector{sanitize_a, transform_x, transform_y})
                   ->sanitizes<Backward>(context.kind_factory->transform_kind(
                       kind_a,
                       context.transforms_factory->create(
                           std::vector{transform_z, sanitize_b}),
                       nullptr)));
  EXPECT_FALSE(context.transforms_factory
                   ->create(std::vector{sanitize_a, transform_x, transform_y})
                   ->sanitizes<Backward>(context.kind_factory->transform_kind(
                       kind_a,
                       context.transforms_factory->create(
                           std::vector{sanitize_b, transform_z}),
                       nullptr)));
}

} // namespace marianatrench

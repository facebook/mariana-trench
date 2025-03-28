/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>
#include <gmock/gmock.h>

#include <mariana-trench/Kind.h>
#include <mariana-trench/MultiSourceMultiSinkRule.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class KindTest : public test::Test {};

TEST_F(KindTest, SerializationDeserialization) {
  auto context = test::make_empty_context();

  const auto* named_kind = context.kind_factory->get("NamedKind");
  EXPECT_EQ(Kind::from_json(named_kind->to_json(), context), named_kind);
  EXPECT_EQ(
      Kind::from_trace_string(named_kind->to_trace_string(), context),
      named_kind);

  const auto* local_return_kind = context.kind_factory->local_return();
  EXPECT_EQ(
      Kind::from_json(local_return_kind->to_json(), context),
      local_return_kind);
  EXPECT_EQ(
      Kind::from_trace_string(local_return_kind->to_trace_string(), context),
      local_return_kind);

  {
    const auto* local_argument_kind = context.kind_factory->local_argument(0);
    EXPECT_EQ(
        Kind::from_json(local_argument_kind->to_json(), context),
        local_argument_kind);
    EXPECT_EQ(
        Kind::from_trace_string(
            local_argument_kind->to_trace_string(), context),
        local_argument_kind);
  }

  {
    // LocalArgument: Validate parsing multiple digit indexes.
    const auto* local_argument_kind = context.kind_factory->local_argument(10);
    EXPECT_EQ(
        Kind::from_json(local_argument_kind->to_json(), context),
        local_argument_kind);
    EXPECT_EQ(
        Kind::from_trace_string(
            local_argument_kind->to_trace_string(), context),
        local_argument_kind);
  }

  {
    // TransformKind: Local transforms only.
    const auto* transform_kind = context.kind_factory->transform_kind(
        named_kind,
        /* local_transforms */
        context.transforms_factory->create({"LocalTransform1"}, context),
        /* global_transforms */ nullptr);
    EXPECT_EQ(
        Kind::from_json(transform_kind->to_json(), context), transform_kind);
  }

  {
    // TransformKind: Global transforms only.
    const auto* transform_kind = context.kind_factory->transform_kind(
        named_kind,
        /* local_transforms */ nullptr,
        /* global_transforms */
        context.transforms_factory->create({"GlobalTransform1"}, context));
    EXPECT_EQ(
        Kind::from_json(transform_kind->to_json(), context), transform_kind);
  }

  {
    // TransformKind: Local and Global transforms.
    const auto* transform_kind = context.kind_factory->transform_kind(
        named_kind,
        /* local_transforms */
        context.transforms_factory->create({"LocalTransform1"}, context),
        /* global_transforms */
        context.transforms_factory->create({"GlobalTransform1"}, context));
    EXPECT_EQ(
        Kind::from_json(transform_kind->to_json(), context), transform_kind);
  }

  {
    // TransformKind: Local and Global transforms, multiple transforms
    const auto* transform_kind = context.kind_factory->transform_kind(
        named_kind,
        /* local_transforms */
        context.transforms_factory->create(
            {"LocalTransform1", "LocalTransform2"}, context),
        /* global_transforms */
        context.transforms_factory->create(
            {"GlobalTransform1", "GlobalTransform2"}, context));
    EXPECT_EQ(
        Kind::from_json(transform_kind->to_json(), context), transform_kind);
  }

  // Deserialize PartialKind.
  const auto* partial_kind =
      context.kind_factory->get_partial("PartialKind", "label");
  EXPECT_EQ(Kind::from_json(partial_kind->to_json(), context), partial_kind);
  EXPECT_THROW(
      Kind::from_trace_string(partial_kind->to_trace_string(), context),
      KindNotSupportedError);

  // Deserialize TriggeredPartialKind.
  auto source_kinds_a =
      Rule::KindSet{context.kind_factory->get("NamedSourceKindA")};
  auto source_kinds_b =
      Rule::KindSet{context.kind_factory->get("NamedSourceKindB")};
  auto partial_kinds = MultiSourceMultiSinkRule::PartialKindSet{
      context.kind_factory->get_partial("Partial", "a"),
      context.kind_factory->get_partial("Partial", "b")};
  const auto* triggered_partial_kind =
      context.kind_factory->get_triggered(partial_kind, 1);
  EXPECT_EQ(
      Kind::from_json(triggered_partial_kind->to_json(), context),
      triggered_partial_kind);
  EXPECT_THROW(
      Kind::from_trace_string(
          triggered_partial_kind->to_trace_string(), context),
      KindNotSupportedError);
}

} // namespace marianatrench

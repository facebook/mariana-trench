/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <exception>
#include <memory>

#include <gmock/gmock.h>

#include <Show.h>

#include <mariana-trench/FieldModel.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class FieldModelTest : public test::Test {};

TEST_F(FieldModelTest, Join) {
  auto context = test::make_empty_context();
  const auto* source_kind = context.kinds->get("TestSource");
  const auto* source_kind2 = context.kinds->get("TestSource2");
  const auto* sink_kind = context.kinds->get("TestSink");

  FieldModel model;
  EXPECT_TRUE(model.sources().is_bottom());
  EXPECT_TRUE(model.sinks().is_bottom());

  // Sources are added.
  FieldModel model_with_source(
      /* field */ nullptr,
      /* sources */
      {test::make_leaf_taint_config(source_kind)},
      /* sinks */ {});
  model.join_with(model_with_source);
  EXPECT_EQ(model.sources(), Taint{test::make_leaf_taint_config(source_kind)});
  EXPECT_TRUE(model.sinks().is_bottom());

  // Repeated application is idempotent.
  model.join_with(model_with_source);
  EXPECT_EQ(model.sources(), Taint{test::make_leaf_taint_config(source_kind)});
  EXPECT_TRUE(model.sinks().is_bottom());

  FieldModel model_with_other_source(
      /* field */ nullptr,
      /* sources */ {test::make_leaf_taint_config(source_kind2)},
      /* sinks */ {});
  model.join_with(model_with_other_source);
  auto source_taint = Taint{
      test::make_leaf_taint_config(source_kind),
      test::make_leaf_taint_config(source_kind2)};
  EXPECT_EQ(model.sources(), source_taint);
  EXPECT_TRUE(model.sinks().is_bottom());

  // Sinks are added.
  FieldModel model_with_sink(
      /* field */ nullptr,
      /* sources */ {},
      /* sinks */
      {test::make_leaf_taint_config(sink_kind)});
  model.join_with(model_with_sink);
  EXPECT_EQ(model.sources(), source_taint);
  EXPECT_EQ(model.sinks(), Taint{test::make_leaf_taint_config(sink_kind)});
}

} // namespace marianatrench

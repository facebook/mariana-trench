/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>

#include <gmock/gmock.h>

#include <Show.h>

#include <mariana-trench/model-generator/ModelGeneratorName.h>
#include <mariana-trench/tests/Test.h>

namespace marianatrench {

class ModelGeneratorNameTest : public test::Test {};

TEST_F(ModelGeneratorNameTest, SerializationDeserialization) {
  auto context = test::make_empty_context();

  {
    auto name = ModelGeneratorName(
        "identifier", /* part */ std::nullopt, /* is_sharded */ false);
    EXPECT_EQ(*ModelGeneratorName::from_json(name.to_json(), context), name);
  }

  {
    auto name =
        ModelGeneratorName("identifier", "part", /* is_sharded */ false);
    EXPECT_EQ(*ModelGeneratorName::from_json(name.to_json(), context), name);
  }

  {
    auto name = ModelGeneratorName(
        "identifier", /* part */ std::nullopt, /* is_sharded */ true);
    EXPECT_EQ(*ModelGeneratorName::from_json(name.to_json(), context), name);
  }

  {
    auto name = ModelGeneratorName("identifier", "part", /* is_sharded */ true);
    EXPECT_EQ(*ModelGeneratorName::from_json(name.to_json(), context), name);
  }

  {
    auto name = ModelGeneratorName(
        "identifier", "part:with:colons", /* is_sharded */ true);
    EXPECT_EQ(*ModelGeneratorName::from_json(name.to_json(), context), name);
  }

  {
    // ':' mess with deserialization if the identifier contains them.
    auto name = ModelGeneratorName(
        "identifier:with:colons", "part", /* is_sharded */ false);
    auto deserialized_name = ModelGeneratorName(
        "identifier", "with:colons:part", /* is_sharded */ false);
    EXPECT_EQ(
        *ModelGeneratorName::from_json(name.to_json(), context),
        deserialized_name);

    // However, the string representation will be the same.
    EXPECT_EQ(show(name), show(deserialized_name));
  }

  {
    // ':' mess with deserialization if the identifier contains them.
    auto name = ModelGeneratorName(
        "identifier:with:colons", "part", /* is_sharded */ true);
    auto deserialized_name = ModelGeneratorName(
        "identifier", "with:colons:part", /* is_sharded */ true);
    EXPECT_EQ(
        *ModelGeneratorName::from_json(name.to_json(), context),
        deserialized_name);

    // However, the string representation will be the same.
    EXPECT_EQ(show(name), show(deserialized_name));
  }
}

} // namespace marianatrench

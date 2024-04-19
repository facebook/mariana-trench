/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/model-generator/ModelGeneratorNameFactory.h>

namespace marianatrench {

ModelGeneratorNameFactory::ModelGeneratorNameFactory() = default;

const ModelGeneratorName* ModelGeneratorNameFactory::create(
    const std::string& identifier) const {
  return set_
      .insert(ModelGeneratorName(
          identifier, /* part */ std::nullopt, /* is_sharded */ false))
      .first;
}

const ModelGeneratorName* ModelGeneratorNameFactory::create(
    const std::string& identifier,
    const std::string& part) const {
  return set_
      .insert(ModelGeneratorName(identifier, part, /* is_sharded */ false))
      .first;
}

const ModelGeneratorName* ModelGeneratorNameFactory::create(
    const std::string& identifier,
    int part) const {
  return set_
      .insert(ModelGeneratorName(
          identifier, std::to_string(part), /* is_sharded */ false))
      .first;
}

const ModelGeneratorName* ModelGeneratorNameFactory::create_sharded(
    const std::string& identifier,
    const ModelGeneratorName* MT_NULLABLE original_generator) const {
  std::optional<std::string> original_generator_name =
      original_generator != nullptr
      ? std::optional<std::string>(show(original_generator))
      : std::nullopt;
  auto name = ModelGeneratorName(
      identifier,
      /* part */ std::move(original_generator_name),
      /* is_sharded */ true);
  return set_.insert(name).first;
}

const ModelGeneratorName* ModelGeneratorNameFactory::create(
    const std::string& identifier,
    std::optional<std::string> part,
    bool is_sharded) const {
  return set_
      .insert(ModelGeneratorName(identifier, std::move(part), is_sharded))
      .first;
}

const ModelGeneratorName* MT_NULLABLE
ModelGeneratorNameFactory::get(const std::string& identifier) const {
  return set_.get(ModelGeneratorName(
      identifier, /* part */ std::nullopt, /* is_sharded */ false));
}

const ModelGeneratorNameFactory& ModelGeneratorNameFactory::singleton() {
  // Thread-safe global variable, initialized on first call.
  static ModelGeneratorNameFactory instance;
  return instance;
}

} // namespace marianatrench

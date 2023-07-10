/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/model-generator/ModelGeneratorNameFactory.h>

namespace marianatrench {

ModelGeneratorNameFactory::ModelGeneratorNameFactory() = default;

const ModelGeneratorName* ModelGeneratorNameFactory::create(
    const std::string& identifier) const {
  return set_.insert(ModelGeneratorName(identifier, /* part */ std::nullopt))
      .first;
}

const ModelGeneratorName* ModelGeneratorNameFactory::create(
    const std::string& identifier,
    const std::string& part) const {
  return set_.insert(ModelGeneratorName(identifier, part)).first;
}

const ModelGeneratorName* ModelGeneratorNameFactory::create(
    const std::string& identifier,
    int part) const {
  return set_.insert(ModelGeneratorName(identifier, std::to_string(part)))
      .first;
}

const ModelGeneratorName* MT_NULLABLE
ModelGeneratorNameFactory::get(const std::string& identifier) const {
  return set_.get(ModelGeneratorName(identifier, /* part */ std::nullopt));
}

const ModelGeneratorNameFactory& ModelGeneratorNameFactory::singleton() {
  // Thread-safe global variable, initialized on first call.
  static ModelGeneratorNameFactory instance;
  return instance;
}

} // namespace marianatrench

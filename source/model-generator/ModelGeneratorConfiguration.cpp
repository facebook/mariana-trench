/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/model-generator/ModelGeneratorConfiguration.h>

namespace marianatrench {

ModelGeneratorConfiguration::ModelGeneratorConfiguration(
    const std::string& name)
    : name_(name) {}

ModelGeneratorConfiguration ModelGeneratorConfiguration::from_json(
    const Json::Value& value) {
  JsonValidation::validate_object(value);

  return ModelGeneratorConfiguration(JsonValidation::string(value, "name"));
}

const std::string& ModelGeneratorConfiguration::name() const {
  return name_;
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/model-generator/ModelGeneratorName.h>
#include <mariana-trench/model-generator/ModelGeneratorNameFactory.h>

namespace marianatrench {

ModelGeneratorName::ModelGeneratorName(
    std::string identifier,
    std::optional<std::string> part)
    : identifier_(std::move(identifier)), part_(std::move(part)) {}

bool ModelGeneratorName::operator==(const ModelGeneratorName& other) const {
  return identifier_ == other.identifier_ && part_ == other.part_;
}

const ModelGeneratorName* ModelGeneratorName::from_json(
    const Json::Value& value,
    Context& context) {
  auto name = JsonValidation::string(value);
  auto separator = name.find(':');
  if (separator != std::string::npos) {
    auto identifier = name.substr(0, separator);
    auto part = name.substr(separator + 1);
    return context.model_generator_name_factory->create(identifier, part);
  }
  return context.model_generator_name_factory->create(name);
}

Json::Value ModelGeneratorName::to_json() const {
  if (part_) {
    return Json::Value(fmt::format("{}:{}", identifier_, *part_));
  } else {
    return Json::Value(identifier_);
  }
}

std::ostream& operator<<(std::ostream& out, const ModelGeneratorName& name) {
  if (name.part_) {
    out << name.identifier_ << ":" << *name.part_;
  } else {
    out << name.identifier_;
  }
  return out;
}

} // namespace marianatrench

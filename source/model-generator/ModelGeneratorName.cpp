/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string/predicate.hpp>
#include <fmt/format.h>

#include <Show.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/model-generator/ModelGeneratorName.h>
#include <mariana-trench/model-generator/ModelGeneratorNameFactory.h>

namespace marianatrench {

ModelGeneratorName::ModelGeneratorName(
    std::string identifier,
    std::optional<std::string> part,
    bool is_sharded)
    : identifier_(std::move(identifier)),
      part_(std::move(part)),
      is_sharded_(is_sharded) {}

bool ModelGeneratorName::operator==(const ModelGeneratorName& other) const {
  return identifier_ == other.identifier_ && part_ == other.part_ &&
      is_sharded_ == other.is_sharded_;
}

const ModelGeneratorName* ModelGeneratorName::from_json(
    const Json::Value& value,
    Context& context) {
  auto name = JsonValidation::string(value);

  bool is_sharded = false;
  if (boost::starts_with(name, "sharded:")) {
    is_sharded = true;
    name = name.substr(std::string("sharded:").length());
  }

  std::string identifier = name;
  std::optional<std::string> part = std::nullopt;

  auto separator = name.find(':');
  if (separator != std::string::npos) {
    identifier = name.substr(0, separator);
    part = name.substr(separator + 1);
  }

  return context.model_generator_name_factory->create(
      identifier, part, is_sharded);
}

Json::Value ModelGeneratorName::to_json() const {
  return Json::Value(show(this));
}

std::ostream& operator<<(std::ostream& out, const ModelGeneratorName& name) {
  if (name.is_sharded_) {
    out << "sharded:";
  }

  if (name.part_) {
    out << name.identifier_ << ":" << *name.part_;
  } else {
    out << name.identifier_;
  }

  return out;
}

} // namespace marianatrench

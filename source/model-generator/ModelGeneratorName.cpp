/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>

#include <mariana-trench/model-generator/ModelGeneratorName.h>

namespace marianatrench {

ModelGeneratorName::ModelGeneratorName(
    std::string identifier,
    std::optional<std::string> part)
    : identifier_(std::move(identifier)), part_(std::move(part)) {}

bool ModelGeneratorName::operator==(const ModelGeneratorName& other) const {
  return identifier_ == other.identifier_ && part_ == other.part_;
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

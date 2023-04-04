/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Feature.h>
#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

const Feature* Feature::from_json(const Json::Value& value, Context& context) {
  return context.feature_factory->get(JsonValidation::string(value));
}

Json::Value Feature::to_json() const {
  return Json::Value(name_);
}

std::ostream& operator<<(std::ostream& out, const Feature& data) {
  return out << data.name_;
}

} // namespace marianatrench

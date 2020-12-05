/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Feature.h>
#include <mariana-trench/Features.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

const Feature* Feature::from_json(const Json::Value& value, Context& context) {
  return context.features->get(JsonValidation::string(value));
}

Json::Value Feature::to_json() const {
  return Json::Value(data_);
}

std::ostream& operator<<(std::ostream& out, const Feature& data) {
  return out << data.data_;
}

} // namespace marianatrench

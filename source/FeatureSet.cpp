/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/FeatureSet.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Show.h>

namespace marianatrench {

FeatureSet FeatureSet::from_json(const Json::Value& value, Context& context) {
  FeatureSet features;
  for (const auto& feature_value : JsonValidation::null_or_array(value)) {
    features.add(Feature::from_json(feature_value, context));
  }
  return features;
}

Json::Value FeatureSet::to_json() const {
  auto features = Json::Value(Json::arrayValue);
  for (const auto* feature : set_) {
    features.append(feature->to_json());
  }
  return features;
}

std::ostream& operator<<(std::ostream& out, const FeatureSet& features) {
  return show_set(out, features);
}

} // namespace marianatrench

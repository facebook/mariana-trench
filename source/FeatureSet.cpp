/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/FeatureSet.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

FeatureSet::FeatureSet(std::initializer_list<const Feature*> features)
    : set_(features) {}

bool FeatureSet::contains(const Feature* feature) const {
  return set_.contains(feature);
}

void FeatureSet::add(const Feature* feature) {
  set_.insert(feature);
}

void FeatureSet::remove(const Feature* feature) {
  set_.remove(feature);
}

bool FeatureSet::leq(const FeatureSet& other) const {
  return set_.is_subset_of(other.set_);
}

bool FeatureSet::equals(const FeatureSet& other) const {
  return set_.equals(other.set_);
}

void FeatureSet::join_with(const FeatureSet& other) {
  set_.union_with(other.set_);
}

void FeatureSet::widen_with(const FeatureSet& other) {
  join_with(other);
}

void FeatureSet::meet_with(const FeatureSet& other) {
  set_.intersection_with(other.set_);
}

void FeatureSet::narrow_with(const FeatureSet& other) {
  meet_with(other);
}

void FeatureSet::difference_with(const FeatureSet& other) {
  set_.difference_with(other.set_);
}

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
  out << "{";
  for (auto iterator = features.begin(), end = features.end();
       iterator != end;) {
    out << "`" << show(*iterator) << "`";
    ++iterator;
    if (iterator != end) {
      out << ", ";
    }
  }
  return out << "}";
}

} // namespace marianatrench

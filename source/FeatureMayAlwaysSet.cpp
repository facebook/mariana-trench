/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/FeatureMayAlwaysSet.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

FeatureMayAlwaysSet::FeatureMayAlwaysSet(
    std::initializer_list<const Feature*> features)
    : set_(features) {}

FeatureMayAlwaysSet::FeatureMayAlwaysSet(
    const FeatureSet& may,
    const FeatureSet& always)
    : set_(/* over */ may.set_, /* under */ always.set_) {}

FeatureMayAlwaysSet FeatureMayAlwaysSet::make_may(
    std::initializer_list<const Feature*> features) {
  auto set = sparta::PatriciaTreeSet<const Feature*>(features);
  return FeatureMayAlwaysSet(OverUnderSet(/* over */ set, /* under */ {}));
}

FeatureMayAlwaysSet FeatureMayAlwaysSet::make_may(const FeatureSet& features) {
  return FeatureMayAlwaysSet(
      OverUnderSet(/* over */ features.set_, /* under */ {}));
}

FeatureMayAlwaysSet FeatureMayAlwaysSet::make_always(
    std::initializer_list<const Feature*> features) {
  auto set = sparta::PatriciaTreeSet<const Feature*>(features);
  return FeatureMayAlwaysSet(OverUnderSet(/* over */ set, /* under */ set));
}

FeatureMayAlwaysSet FeatureMayAlwaysSet::make_always(
    const FeatureSet& features) {
  return FeatureMayAlwaysSet(
      OverUnderSet(/* over */ features.set_, /* under */ features.set_));
}

FeatureSet FeatureMayAlwaysSet::may() const {
  mt_assert(set_.is_value());
  return FeatureSet(set_.over());
}

FeatureSet FeatureMayAlwaysSet::always() const {
  mt_assert(set_.is_value());
  return FeatureSet(set_.under());
}

void FeatureMayAlwaysSet::add_may(const Feature* feature) {
  set_.add_over(feature);
}

void FeatureMayAlwaysSet::add_may(const FeatureSet& features) {
  set_.add_over(features.set_);
}

void FeatureMayAlwaysSet::add_always(const Feature* feature) {
  set_.add_under(feature);
}

void FeatureMayAlwaysSet::add_always(const FeatureSet& features) {
  set_.add_under(features.set_);
}

void FeatureMayAlwaysSet::add(const FeatureMayAlwaysSet& other) {
  set_.add(other.set_);
}

bool FeatureMayAlwaysSet::leq(const FeatureMayAlwaysSet& other) const {
  return set_.leq(other.set_);
}

bool FeatureMayAlwaysSet::equals(const FeatureMayAlwaysSet& other) const {
  return set_.equals(other.set_);
}

void FeatureMayAlwaysSet::join_with(const FeatureMayAlwaysSet& other) {
  set_.join_with(other.set_);
}

void FeatureMayAlwaysSet::widen_with(const FeatureMayAlwaysSet& other) {
  set_.widen_with(other.set_);
}

void FeatureMayAlwaysSet::meet_with(const FeatureMayAlwaysSet& other) {
  set_.meet_with(other.set_);
}

void FeatureMayAlwaysSet::narrow_with(const FeatureMayAlwaysSet& other) {
  set_.narrow_with(other.set_);
}

FeatureMayAlwaysSet FeatureMayAlwaysSet::from_json(
    const Json::Value& value,
    Context& context) {
  auto may_features = FeatureSet{};
  auto always_features = FeatureSet{};

  JsonValidation::validate_object(value);
  if (value.isMember("may_features")) {
    JsonValidation::null_or_array(value, /* field */ "may_features");
    may_features.join_with(
        FeatureSet::from_json(value["may_features"], context));
  }
  if (value.isMember("always_features")) {
    JsonValidation::null_or_array(value, /* field */ "always_features");
    always_features.join_with(
        FeatureSet::from_json(value["always_features"], context));
  }

  return FeatureMayAlwaysSet(may_features, always_features);
}

Json::Value FeatureMayAlwaysSet::to_json() const {
  mt_assert(set_.is_value());

  auto may_features = may();
  auto always_features = always();
  may_features.difference_with(always_features);

  auto value = Json::Value(Json::objectValue);
  if (!may_features.empty()) {
    value["may_features"] = may_features.to_json();
  }
  if (!always_features.empty()) {
    value["always_features"] = always_features.to_json();
  }

  return value;
}

std::ostream& operator<<(
    std::ostream& out,
    const FeatureMayAlwaysSet& features) {
  if (features.is_top()) {
    return out << "T";
  } else if (features.is_bottom()) {
    return out << "_|_";
  } else if (features.empty()) {
    return out << "{}";
  } else {
    return out << "{may=" << features.may() << ", always=" << features.always()
               << "}";
  }
}

} // namespace marianatrench

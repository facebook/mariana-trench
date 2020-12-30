/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Propagation.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

bool Propagation::leq(const Propagation& other) const {
  if (is_bottom()) {
    return true;
  } else if (other.is_bottom()) {
    return false;
  } else {
    return input_.leq(other.input_) &&
        inferred_features_.leq(other.inferred_features_) &&
        user_features_.leq(other.user_features_);
  }
}

bool Propagation::equals(const Propagation& other) const {
  if (is_bottom()) {
    return other.is_bottom();
  } else if (other.is_bottom()) {
    return false;
  } else {
    return input_ == other.input_ &&
        inferred_features_ == other.inferred_features_ &&
        user_features_ == other.user_features_;
  }
}

void Propagation::join_with(const Propagation& other) {
  if (is_bottom()) {
    *this = other;
  } else if (other.is_bottom()) {
    return;
  } else {
    input_.join_with(other.input_);
    inferred_features_.join_with(other.inferred_features_);
    user_features_.join_with(other.user_features_);
  }
}

void Propagation::widen_with(const Propagation& other) {
  join_with(other);
}

void Propagation::meet_with(const Propagation& /*other*/) {
  // Not implemented.
}

void Propagation::narrow_with(const Propagation& other) {
  meet_with(other);
}

FeatureMayAlwaysSet Propagation::features() const {
  if (inferred_features_.is_bottom()) {
    return FeatureMayAlwaysSet::make_always(user_features_);
  }

  auto features = inferred_features_;
  features.add_always(user_features_);
  mt_assert(!features.is_bottom());
  return features;
}

Propagation Propagation::from_json(const Json::Value& value, Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::string(value, /* field */ "input");
  auto input = AccessPath::from_json(value["input"]);

  if (!input.root().is_argument()) {
    throw JsonValidationError(
        value, /* field */ "input", "an access path to an argument");
  }

  auto inferred_features = FeatureMayAlwaysSet::from_json(value, context);
  auto user_features = FeatureSet::from_json(value["features"], context);

  return Propagation(input, inferred_features, user_features);
}

Json::Value Propagation::to_json() const {
  auto value = Json::Value(Json::objectValue);
  value["input"] = input_.to_json();
  JsonValidation::update_object(value, features().to_json());
  return value;
}

bool Propagation::GroupEqual::operator()(
    const Propagation& left,
    const Propagation& right) const {
  return left.input_.root() == right.input_.root();
}

std::size_t Propagation::GroupHash::operator()(
    const Propagation& propagation) const {
  return propagation.input_.root().encode();
}

std::ostream& operator<<(std::ostream& out, const Propagation& propagation) {
  return out << "Propagation(input=" << propagation.input_
             << ", inferred_features=" << propagation.inferred_features_
             << ", user_features=" << propagation.user_features_ << ")";
}

} // namespace marianatrench

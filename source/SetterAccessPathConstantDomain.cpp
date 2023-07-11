/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/SetterAccessPathConstantDomain.h>

namespace marianatrench {

SetterAccessPath::SetterAccessPath(AccessPath target, AccessPath value)
    : target_(std::move(target)), value_(std::move(value)) {}

bool SetterAccessPath::operator==(const SetterAccessPath& other) const {
  return target_ == other.target_ && value_ == other.value_;
}

SetterAccessPath SetterAccessPath::from_json(const Json::Value& value) {
  JsonValidation::validate_object(value);
  JsonValidation::check_unexpected_members(value, {"target", "value"});

  JsonValidation::string(value, /* field */ "target");
  auto setter_target = AccessPath::from_json(value["target"]);

  JsonValidation::string(value, /* field */ "value");
  auto setter_value = AccessPath::from_json(value["value"]);

  return SetterAccessPath(setter_target, setter_value);
}

Json::Value SetterAccessPath::to_json() const {
  auto value = Json::Value(Json::objectValue);
  value["target"] = target_.to_json();
  value["value"] = value_.to_json();
  return value;
}

std::ostream& operator<<(std::ostream& out, const SetterAccessPath& setter) {
  out << "SetterAccessPath(target=" << setter.target_
      << ", value=" << setter.value_ << ")";
  return out;
}

} // namespace marianatrench

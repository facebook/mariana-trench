/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <json/json.h>

#include <sparta/ConstantAbstractDomain.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

/* Canonical representation of a setter method. */
class SetterAccessPath final {
 public:
  /* Default constructor required by sparta, do not use. */
  explicit SetterAccessPath() = default;

  SetterAccessPath(AccessPath target, AccessPath value);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(SetterAccessPath)

  bool operator==(const SetterAccessPath& other) const;

  const AccessPath& target() const {
    return target_;
  }
  const AccessPath& value() const {
    return value_;
  }

  static SetterAccessPath from_json(const Json::Value& value);
  Json::Value to_json() const;

 private:
  friend std::ostream& operator<<(
      std::ostream& out,
      const SetterAccessPath& access_path);

 private:
  AccessPath target_;
  AccessPath value_;
};

using SetterAccessPathConstantDomain =
    sparta::ConstantAbstractDomain<SetterAccessPath>;

} // namespace marianatrench

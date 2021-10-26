/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/JsonValidation.h>

namespace marianatrench {
class IntegerConstraint final {
 public:
  enum class Operator { LT, LE, GT, GE, NE, EQ };

  IntegerConstraint(int rhs, Operator _operator);
  bool satisfy(int lhs) const;
  bool operator==(const IntegerConstraint& other) const;

  static IntegerConstraint from_json(const Json::Value& constraint);

 private:
  int rhs_;
  Operator operator_;
};
} // namespace marianatrench

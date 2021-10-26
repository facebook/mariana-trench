/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Assert.h>
#include <mariana-trench/model-generator/IntegerConstraint.h>

namespace marianatrench {
IntegerConstraint::IntegerConstraint(int rhs, Operator _operator)
    : rhs_(rhs), operator_(_operator) {}

bool IntegerConstraint::satisfy(int lhs) const {
  switch (operator_) {
    case Operator::LT: {
      return lhs < rhs_;
    }
    case Operator::LE: {
      return lhs <= rhs_;
    }
    case Operator::GT: {
      return lhs > rhs_;
    }
    case Operator::GE: {
      return lhs >= rhs_;
    }
    case Operator::NE: {
      return lhs != rhs_;
    }
    case Operator::EQ: {
      return lhs == rhs_;
    }
    default: {
      mt_unreachable();
      return true;
    }
  }
}

bool IntegerConstraint::operator==(const IntegerConstraint& other) const {
  return other.rhs_ == rhs_ && other.operator_ == operator_;
}

IntegerConstraint IntegerConstraint::from_json(const Json::Value& constraint) {
  JsonValidation::validate_object(constraint);

  auto constraint_name = JsonValidation::string(constraint, "constraint");
  auto rhs = JsonValidation::integer(constraint, "value");
  if (constraint_name == "==") {
    return IntegerConstraint(rhs, IntegerConstraint::Operator::EQ);
  } else if (constraint_name == "!=") {
    return IntegerConstraint(rhs, IntegerConstraint::Operator::NE);
  } else if (constraint_name == "<=") {
    return IntegerConstraint(rhs, IntegerConstraint::Operator::LE);
  } else if (constraint_name == "<") {
    return IntegerConstraint(rhs, IntegerConstraint::Operator::LT);
  } else if (constraint_name == ">=") {
    return IntegerConstraint(rhs, IntegerConstraint::Operator::GE);
  } else if (constraint_name == ">") {
    return IntegerConstraint(rhs, IntegerConstraint::Operator::GT);
  } else {
    throw JsonValidationError(
        constraint,
        /* field */ "constraint",
        /* expected */ "< | <= | == | > | >= | !=");
  }
}
} // namespace marianatrench

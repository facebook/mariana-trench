/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/constraints/ParameterConstraints.h>

namespace marianatrench {

TypeParameterConstraint::TypeParameterConstraint(
    std::unique_ptr<TypeConstraint> inner_constraint)
    : inner_constraint_(std::move(inner_constraint)) {}

bool TypeParameterConstraint::satisfy(
    const DexAnnotationSet* MT_NULLABLE /* unused */,
    const DexType* type) const {
  return type ? inner_constraint_->satisfy(type) : false;
}

bool TypeParameterConstraint::operator==(
    const ParameterConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const TypeParameterConstraint*>(&other)) {
    return *(other_constraint->inner_constraint_) == *inner_constraint_;
  } else {
    return false;
  }
}

std::unique_ptr<ParameterConstraint> ParameterConstraint::from_json(
    const Json::Value& constraint) {
  JsonValidation::validate_object(constraint);

  std::string constraint_name =
      JsonValidation::string(constraint, "constraint");
  return std::make_unique<TypeParameterConstraint>(
      TypeConstraint::from_json(constraint));
}

} // namespace marianatrench

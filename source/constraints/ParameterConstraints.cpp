/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/constraints/MethodConstraints.h>
#include <mariana-trench/constraints/ParameterConstraints.h>

namespace marianatrench {

AllOfParameterConstraint::AllOfParameterConstraint(
    std::vector<std::unique_ptr<ParameterConstraint>> constraints)
    : constraints_(std::move(constraints)) {}

bool AllOfParameterConstraint::satisfy(
    const DexAnnotationSet* MT_NULLABLE annotations_set,
    const DexType* type) const {
  return std::all_of(
      constraints_.begin(),
      constraints_.end(),
      [annotations_set, type](const auto& constraint) {
        return constraint->satisfy(annotations_set, type);
      });
}

bool AllOfParameterConstraint::operator==(
    const ParameterConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const AllOfParameterConstraint*>(&other)) {
    return std::is_permutation(
        other_constraint->constraints_.begin(),
        other_constraint->constraints_.end(),
        constraints_.begin(),
        constraints_.end(),
        [](const auto& left, const auto& right) { return *left == *right; });
  } else {
    return false;
  }
}

AnyOfParameterConstraint::AnyOfParameterConstraint(
    std::vector<std::unique_ptr<ParameterConstraint>> constraints)
    : constraints_(std::move(constraints)) {}

bool AnyOfParameterConstraint::satisfy(
    const DexAnnotationSet* MT_NULLABLE annotations_set,
    const DexType* type) const {
  // If there is no constraint, the method vacuously satisfies the constraint
  // This is different from the semantic of std::any_of
  if (constraints_.empty()) {
    return true;
  } else {
    return std::any_of(
        constraints_.begin(),
        constraints_.end(),
        [annotations_set, type](const auto& constraint) {
          return constraint->satisfy(annotations_set, type);
        });
  }
}

bool AnyOfParameterConstraint::operator==(
    const ParameterConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const AnyOfParameterConstraint*>(&other)) {
    return std::is_permutation(
        other_constraint->constraints_.begin(),
        other_constraint->constraints_.end(),
        constraints_.begin(),
        constraints_.end(),
        [](const auto& left, const auto& right) { return *left == *right; });
  } else {
    return false;
  }
}

NotParameterConstraint::NotParameterConstraint(
    std::unique_ptr<ParameterConstraint> constraint)
    : constraint_(std::move(constraint)) {}

bool NotParameterConstraint::satisfy(
    const DexAnnotationSet* MT_NULLABLE annotations_set,
    const DexType* type) const {
  return !constraint_->satisfy(annotations_set, type);
}

bool NotParameterConstraint::operator==(
    const ParameterConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const NotParameterConstraint*>(&other)) {
    return *(other_constraint->constraint_) == *constraint_;
  } else {
    return false;
  }
}

HasAnnotationParameterConstraint::HasAnnotationParameterConstraint(
    std::string type,
    std::optional<std::string> annotation)
    : type_(std::move(type)), annotation_(std::move(annotation)) {}

bool HasAnnotationParameterConstraint::satisfy(
    const DexAnnotationSet* MT_NULLABLE annotations_set,
    const DexType* /* unused */) const {
  return annotations_set ? has_annotation(annotations_set, type_, annotation_)
                         : false;
}

bool HasAnnotationParameterConstraint::operator==(
    const ParameterConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const HasAnnotationParameterConstraint*>(&other)) {
    return other_constraint->type_ == type_ &&
        ((!other_constraint->annotation_ && !annotation_) ||
         (other_constraint->annotation_ && annotation_ &&
          other_constraint->annotation_->pattern() == annotation_->pattern()));
  } else {
    return false;
  }
}

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
  if (constraint_name == "any_of" || constraint_name == "all_of") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "inners"});
    std::vector<std::unique_ptr<ParameterConstraint>> constraints;
    for (const auto& inner :
         JsonValidation::null_or_array(constraint, /* field */ "inners")) {
      constraints.push_back(ParameterConstraint::from_json(inner));
    }
    if (constraint_name == "any_of") {
      return std::make_unique<AnyOfParameterConstraint>(std::move(constraints));
    } else {
      return std::make_unique<AllOfParameterConstraint>(std::move(constraints));
    }
  } else if (constraint_name == "not") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "inner"});
    return std::make_unique<NotParameterConstraint>(
        ParameterConstraint::from_json(
            JsonValidation::object(constraint, /* field */ "inner")));
  } else if (constraint_name == "parameter_has_annotation") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "type", "pattern"});
    return std::make_unique<HasAnnotationParameterConstraint>(
        JsonValidation::string(constraint, "type"),
        constraint.isMember("pattern")
            ? std::optional<std::string>{JsonValidation::string(
                  constraint, "pattern")}
            : std::nullopt);
  } else {
    return std::make_unique<TypeParameterConstraint>(
        TypeConstraint::from_json(constraint));
  }
}

} // namespace marianatrench

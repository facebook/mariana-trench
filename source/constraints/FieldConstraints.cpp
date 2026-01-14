/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/constraints/FieldConstraints.h>
#include <mariana-trench/constraints/MethodConstraints.h>

namespace marianatrench {

IsStaticFieldConstraint::IsStaticFieldConstraint(bool expected)
    : expected_(expected) {}

bool IsStaticFieldConstraint::satisfy(const Field* field) const {
  return ((field->dex_field()->get_access() & DexAccessFlags::ACC_STATIC) >
          0) == expected_;
}

bool IsStaticFieldConstraint::operator==(const FieldConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const IsStaticFieldConstraint*>(&other)) {
    return other_constraint->expected_ == expected_;
  } else {
    return false;
  }
}

FieldNameConstraint::FieldNameConstraint(const std::string& regex_string)
    : pattern_(regex_string) {}

bool FieldNameConstraint::satisfy(const Field* field) const {
  return re2::RE2::FullMatch(field->get_name(), pattern_);
}

bool FieldNameConstraint::operator==(const FieldConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const FieldNameConstraint*>(&other)) {
    return other_constraint->pattern_.pattern() == pattern_.pattern();
  } else {
    return false;
  }
}

SignaturePatternFieldConstraint::SignaturePatternFieldConstraint(
    const std::string& regex_string)
    : pattern_(regex_string) {}

bool SignaturePatternFieldConstraint::satisfy(const Field* field) const {
  return re2::RE2::FullMatch(field->show(), pattern_);
}

bool SignaturePatternFieldConstraint::operator==(
    const FieldConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const SignaturePatternFieldConstraint*>(&other)) {
    return other_constraint->pattern_.pattern() == pattern_.pattern();
  } else {
    return false;
  }
}

HasAnnotationFieldConstraint::HasAnnotationFieldConstraint(
    const std::string& type,
    const std::optional<std::string>& annotation)
    : type_(type), annotation_(annotation) {}

bool HasAnnotationFieldConstraint::satisfy(const Field* field) const {
  return has_annotation(field->dex_field()->get_anno_set(), type_, annotation_);
}

bool HasAnnotationFieldConstraint::operator==(
    const FieldConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const HasAnnotationFieldConstraint*>(&other)) {
    return other_constraint->type_ == type_ &&
        ((!other_constraint->annotation_ && !annotation_) ||
         (other_constraint->annotation_ && annotation_ &&
          other_constraint->annotation_->pattern() == annotation_->pattern()));
  } else {
    return false;
  }
}

ParentFieldConstraint::ParentFieldConstraint(
    std::unique_ptr<TypeConstraint> inner_constraint)
    : inner_constraint_(std::move(inner_constraint)) {}

bool ParentFieldConstraint::satisfy(const Field* field) const {
  return inner_constraint_->satisfy(field->get_class());
}

bool ParentFieldConstraint::operator==(const FieldConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const ParentFieldConstraint*>(&other)) {
    return *(other_constraint->inner_constraint_) == *inner_constraint_;
  } else {
    return false;
  }
}

AllOfFieldConstraint::AllOfFieldConstraint(
    std::vector<std::unique_ptr<FieldConstraint>> constraints)
    : constraints_(std::move(constraints)) {}

bool AllOfFieldConstraint::satisfy(const Field* field) const {
  return std::all_of(
      constraints_.begin(),
      constraints_.end(),
      [field](const auto& constraint) { return constraint->satisfy(field); });
}

bool AllOfFieldConstraint::operator==(const FieldConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const AllOfFieldConstraint*>(&other)) {
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

NotFieldConstraint::NotFieldConstraint(
    std::unique_ptr<FieldConstraint> constraint)
    : constraint_(std::move(constraint)) {}

bool NotFieldConstraint::satisfy(const Field* field) const {
  return !constraint_->satisfy(field);
}

bool NotFieldConstraint::operator==(const FieldConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const NotFieldConstraint*>(&other)) {
    return *(other_constraint->constraint_) == *constraint_;
  } else {
    return false;
  }
}

AnyOfFieldConstraint::AnyOfFieldConstraint(
    std::vector<std::unique_ptr<FieldConstraint>> constraints)
    : constraints_(std::move(constraints)) {}

bool AnyOfFieldConstraint::satisfy(const Field* field) const {
  // If there is no constraint, the field vacuously satisfies the constraint
  // This is different from the semantic of std::any_of
  if (constraints_.empty()) {
    return true;
  }
  return std::any_of(
      constraints_.begin(),
      constraints_.end(),
      [field](const auto& constraint) { return constraint->satisfy(field); });
}

bool AnyOfFieldConstraint::operator==(const FieldConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const AnyOfFieldConstraint*>(&other)) {
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

namespace {

std::unique_ptr<FieldConstraint> signature_match_field_name_constraint(
    std::string name) {
  return std::make_unique<FieldNameConstraint>(name);
}

std::unique_ptr<FieldConstraint> signature_match_field_name_constraint(
    const Json::Value& names) {
  std::vector<std::unique_ptr<FieldConstraint>> field_name_constraints;
  for (const auto& name : names) {
    field_name_constraints.push_back(
        signature_match_field_name_constraint(JsonValidation::string(name)));
  }
  return std::make_unique<AnyOfFieldConstraint>(
      std::move(field_name_constraints));
}

std::unique_ptr<FieldConstraint> signature_match_field_parent_constraint(
    const Json::Value& parents) {
  std::vector<std::unique_ptr<FieldConstraint>> parent_name_constraints;
  for (const auto& parent : parents) {
    parent_name_constraints.push_back(
        std::make_unique<ParentFieldConstraint>(
            TypeNameConstraint::from_json(parent)));
  }
  return std::make_unique<AnyOfFieldConstraint>(
      std::move(parent_name_constraints));
}

std::unique_ptr<FieldConstraint>
signature_match_field_parent_extends_constraint(
    const Json::Value& parents,
    bool includes_self) {
  std::vector<std::unique_ptr<FieldConstraint>> parent_extends_constraints;
  for (const auto& parent : parents) {
    parent_extends_constraints.push_back(
        std::make_unique<ParentFieldConstraint>(
            std::make_unique<ExtendsConstraint>(
                TypeNameConstraint::from_json(parent), includes_self)));
  }
  return std::make_unique<AnyOfFieldConstraint>(
      std::move(parent_extends_constraints));
}

} // namespace

std::unique_ptr<FieldConstraint> FieldConstraint::from_json(
    const Json::Value& constraint) {
  JsonValidation::validate_object(constraint);

  std::string constraint_name =
      JsonValidation::string(constraint, "constraint");
  if (constraint_name == "name") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "pattern"});
    return std::make_unique<FieldNameConstraint>(
        JsonValidation::string(constraint, /* field */ "pattern"));
  } else if (constraint_name == "signature") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "pattern"});
    return std::make_unique<SignaturePatternFieldConstraint>(
        JsonValidation::string(constraint, /* field */ "pattern"));
  } else if (constraint_name == "signature_pattern") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "pattern"});
    return std::make_unique<SignaturePatternFieldConstraint>(
        JsonValidation::string(constraint, /* field */ "pattern"));
  } else if (constraint_name == "is_static") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "value"});
    bool expected = constraint.isMember("value")
        ? JsonValidation::boolean(constraint, /* field */ "value")
        : true;
    return std::make_unique<IsStaticFieldConstraint>(expected);
  } else if (constraint_name == "not") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "inner"});
    return std::make_unique<NotFieldConstraint>(FieldConstraint::from_json(
        JsonValidation::object(constraint, /* field */ "inner")));
  } else if (constraint_name == "any_of" || constraint_name == "all_of") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "inners"});
    std::vector<std::unique_ptr<FieldConstraint>> constraints;
    for (const auto& inner :
         JsonValidation::null_or_array(constraint, /* field */ "inners")) {
      constraints.push_back(FieldConstraint::from_json(inner));
    }
    if (constraint_name == "any_of") {
      return std::make_unique<AnyOfFieldConstraint>(std::move(constraints));
    } else {
      return std::make_unique<AllOfFieldConstraint>(std::move(constraints));
    }
  } else if (constraint_name == "has_annotation") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "type", "pattern"});
    return std::make_unique<HasAnnotationFieldConstraint>(
        JsonValidation::string(constraint, "type"),
        constraint.isMember("pattern")
            ? std::optional<std::string>{JsonValidation::string(
                  constraint, "pattern")}
            : std::nullopt);
  } else if (constraint_name == "parent") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "type", "inner"});
    return std::make_unique<ParentFieldConstraint>(TypeConstraint::from_json(
        JsonValidation::object(constraint, /* field */ "inner")));
  } else if (constraint_name == "signature_match") {
    JsonValidation::check_unexpected_members(
        constraint,
        {"constraint",
         "name",
         "names",
         "parent",
         "parents",
         "extends",
         "include_self"});
    std::vector<std::unique_ptr<FieldConstraint>> constraints;
    int name_count = 0;
    int parent_count = 0;
    if (constraint.isMember("name")) {
      name_count++;
      constraints.push_back(signature_match_field_name_constraint(
          JsonValidation::string(constraint, /* field */ "name")));
    }
    if (constraint.isMember("names")) {
      name_count++;
      constraints.push_back(signature_match_field_name_constraint(
          JsonValidation::nonempty_array(constraint, /* field */ "names")));
    }
    if (constraint.isMember("parent")) {
      parent_count++;
      constraints.push_back(
          std::make_unique<ParentFieldConstraint>(
              TypeNameConstraint::from_json(constraint["parent"])));
    }
    if (constraint.isMember("parents")) {
      parent_count++;
      constraints.push_back(signature_match_field_parent_constraint(
          JsonValidation::nonempty_array(constraint, /* field */ "parents")));
    }
    if (constraint.isMember("extends")) {
      parent_count++;
      bool includes_self = constraint.isMember("include_self")
          ? JsonValidation::boolean(constraint, /* field */ "include_self")
          : true;
      const auto& extends_field = constraint["extends"];
      if (extends_field.isString()) {
        constraints.push_back(
            std::make_unique<ParentFieldConstraint>(
                std::make_unique<ExtendsConstraint>(
                    TypeNameConstraint::from_json(constraint["extends"]),
                    includes_self)));
      } else {
        constraints.push_back(signature_match_field_parent_extends_constraint(
            JsonValidation::nonempty_array(constraint, /* field */ "extends"),
            includes_self));
      }
    }
    if (parent_count != 1) {
      throw JsonValidationError(
          constraint,
          /* field */ "parents",
          "Exactly one of \"parent\", \"parents\" and \"extends\" should be present.");
    }
    if (name_count != 1) {
      throw JsonValidationError(
          constraint,
          /* field */ "name",
          "Exactly one of \"name\" and \"names\" should be present.");
    }
    return std::make_unique<AllOfFieldConstraint>(std::move(constraints));
  } else {
    throw JsonValidationError(
        constraint,
        /* field */ "constraint",
        /* expected */ "valid field constraint type");
    return nullptr;
  }
}

} // namespace marianatrench

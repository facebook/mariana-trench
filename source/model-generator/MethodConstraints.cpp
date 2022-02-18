/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/RE2.h>
#include <mariana-trench/model-generator/MethodConstraints.h>
#include <mariana-trench/model-generator/TypeConstraints.h>

namespace marianatrench {

bool has_annotation(
    const DexAnnotationSet* annotations_set,
    const std::string& expected_type,
    const std::optional<re2::RE2>& expected_annotation) {
  if (!annotations_set) {
    return false;
  }

  for (const auto& annotation : annotations_set->get_annotations()) {
    DexType* annotation_type = annotation->type();
    if (!annotation_type || annotation_type->str() != expected_type) {
      continue;
    }

    // If annotation is not specified, then we have found the annotation
    // satisfying the specified type
    if (!expected_annotation) {
      return true;
    }
    for (const auto& element : annotation->anno_elems()) {
      if (re2::RE2::FullMatch(
              element.encoded_value->show(), *expected_annotation)) {
        LOG(4,
            "Found annotation type {} value {}.",
            annotation_type->str(),
            element.encoded_value->show());
        return true;
      }
    }
  }
  return false;
}

MethodNameConstraint::MethodNameConstraint(const std::string& regex_string)
    : pattern_(regex_string) {}

MethodHashedSet MethodNameConstraint::may_satisfy(
    const MethodMappings& method_mappings) const {
  auto string_pattern = as_string_literal(pattern_);
  if (!string_pattern) {
    return MethodHashedSet::top();
  }
  return method_mappings.name_to_methods.get(
      *string_pattern, MethodHashedSet::bottom());
}

bool MethodNameConstraint::satisfy(const Method* method) const {
  return re2::RE2::FullMatch(method->get_name(), pattern_);
}

bool MethodNameConstraint::operator==(const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const MethodNameConstraint*>(&other)) {
    return other_constraint->pattern_.pattern() == pattern_.pattern();
  } else {
    return false;
  }
}

ParentConstraint::ParentConstraint(
    std::unique_ptr<TypeConstraint> inner_constraint)
    : inner_constraint_(std::move(inner_constraint)) {}

MethodHashedSet ParentConstraint::may_satisfy(
    const MethodMappings& method_mappings) const {
  return inner_constraint_->may_satisfy(
      method_mappings, MaySatisfyMethodConstraintKind::Parent);
}

bool ParentConstraint::satisfy(const Method* method) const {
  return inner_constraint_->satisfy(method->get_class());
}

bool ParentConstraint::operator==(const MethodConstraint& other) const {
  if (auto* other_constraint = dynamic_cast<const ParentConstraint*>(&other)) {
    return *(other_constraint->inner_constraint_) == *inner_constraint_;
  } else {
    return false;
  }
}

AllOfMethodConstraint::AllOfMethodConstraint(
    std::vector<std::unique_ptr<MethodConstraint>> constraints)
    : constraints_(std::move(constraints)) {}

bool AllOfMethodConstraint::has_children() const {
  return true;
}

std::vector<const MethodConstraint*> AllOfMethodConstraint::children() const {
  std::vector<const MethodConstraint*> constraints;
  for (const auto& constraint : constraints_) {
    constraints.push_back(constraint.get());
  }
  return constraints;
}

MethodHashedSet AllOfMethodConstraint::may_satisfy(
    const MethodMappings& method_mappings) const {
  auto intersection_set = MethodHashedSet::top();
  for (const auto& constraint : constraints_) {
    intersection_set.meet_with(constraint->may_satisfy(method_mappings));
  }
  return intersection_set;
}

bool AllOfMethodConstraint::satisfy(const Method* method) const {
  return std::all_of(
      constraints_.begin(),
      constraints_.end(),
      [method](const auto& constraint) { return constraint->satisfy(method); });
}

bool AllOfMethodConstraint::operator==(const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const AllOfMethodConstraint*>(&other)) {
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

AnyOfMethodConstraint::AnyOfMethodConstraint(
    std::vector<std::unique_ptr<MethodConstraint>> constraints)
    : constraints_(std::move(constraints)) {}

bool AnyOfMethodConstraint::has_children() const {
  return true;
}

std::vector<const MethodConstraint*> AnyOfMethodConstraint::children() const {
  std::vector<const MethodConstraint*> constraints;
  for (const auto& constraint : constraints_) {
    constraints.push_back(constraint.get());
  }
  return constraints;
}

MethodHashedSet AnyOfMethodConstraint::may_satisfy(
    const MethodMappings& method_mappings) const {
  if (constraints_.empty()) {
    return MethodHashedSet::top();
  }
  auto union_set = MethodHashedSet::bottom();
  for (const auto& constraint : constraints_) {
    union_set.join_with(constraint->may_satisfy(method_mappings));
  }
  return union_set;
}

bool AnyOfMethodConstraint::satisfy(const Method* method) const {
  // If there is no constraint, the method vacuously satisfies the constraint
  // This is different from the semantic of std::any_of
  if (constraints_.empty()) {
    return true;
  } else {
    return std::any_of(
        constraints_.begin(),
        constraints_.end(),
        [method](const auto& constraint) {
          return constraint->satisfy(method);
        });
  }
}

bool AnyOfMethodConstraint::operator==(const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const AnyOfMethodConstraint*>(&other)) {
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

NotMethodConstraint::NotMethodConstraint(
    std::unique_ptr<MethodConstraint> constraint)
    : constraint_(std::move(constraint)) {}

bool NotMethodConstraint::has_children() const {
  return true;
}

std::vector<const MethodConstraint*> NotMethodConstraint::children() const {
  return {constraint_.get()};
}

MethodHashedSet NotMethodConstraint::may_satisfy(
    const MethodMappings& method_mappings) const {
  MethodHashedSet child_methods = constraint_->may_satisfy(method_mappings);
  if (child_methods.is_top() || child_methods.is_bottom()) {
    return MethodHashedSet::top();
  }
  MethodHashedSet all_methods = method_mappings.all_methods;
  all_methods.difference_with(child_methods);
  return all_methods;
}

bool NotMethodConstraint::satisfy(const Method* method) const {
  return !constraint_->satisfy(method);
}

bool NotMethodConstraint::operator==(const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const NotMethodConstraint*>(&other)) {
    return *(other_constraint->constraint_) == *constraint_;
  } else {
    return false;
  }
}

NumberParametersConstraint::NumberParametersConstraint(
    IntegerConstraint constraint)
    : constraint_(constraint){};

bool NumberParametersConstraint::satisfy(const Method* method) const {
  return constraint_.satisfy(method->number_of_parameters());
}

bool NumberParametersConstraint::operator==(
    const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const NumberParametersConstraint*>(&other)) {
    return other_constraint->constraint_ == constraint_;
  } else {
    return false;
  }
}

NumberOverridesConstraint::NumberOverridesConstraint(
    IntegerConstraint constraint,
    Context& context)
    : constraint_(constraint), context_(context){};

bool NumberOverridesConstraint::satisfy(const Method* method) const {
  return constraint_.satisfy(context_.overrides->get(method).size());
}

bool NumberOverridesConstraint::operator==(
    const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const NumberOverridesConstraint*>(&other)) {
    return other_constraint->constraint_ == constraint_;
  } else {
    return false;
  }
}

IsStaticConstraint::IsStaticConstraint(bool expected) : expected_(expected) {}

bool IsStaticConstraint::satisfy(const Method* method) const {
  return method->is_static() == expected_;
}

bool IsStaticConstraint::operator==(const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const IsStaticConstraint*>(&other)) {
    return other_constraint->expected_ == expected_;
  } else {
    return false;
  }
}

IsConstructorConstraint::IsConstructorConstraint(bool expected)
    : expected_(expected) {}

bool IsConstructorConstraint::satisfy(const Method* method) const {
  return method->is_constructor() == expected_;
}

bool IsConstructorConstraint::operator==(const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const IsConstructorConstraint*>(&other)) {
    return other_constraint->expected_ == expected_;
  } else {
    return false;
  }
}

IsNativeConstraint::IsNativeConstraint(bool expected) : expected_(expected) {}

bool IsNativeConstraint::satisfy(const Method* method) const {
  return method->is_native() == expected_;
}

bool IsNativeConstraint::operator==(const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const IsNativeConstraint*>(&other)) {
    return other_constraint->expected_ == expected_;
  } else {
    return false;
  }
}

HasCodeConstraint::HasCodeConstraint(bool expected) : expected_(expected) {}

bool HasCodeConstraint::satisfy(const Method* method) const {
  return (method->get_code() != nullptr) == expected_;
}

bool HasCodeConstraint::operator==(const MethodConstraint& other) const {
  if (auto* other_constraint = dynamic_cast<const HasCodeConstraint*>(&other)) {
    return other_constraint->expected_ == expected_;
  } else {
    return false;
  }
}

HasAnnotationMethodConstraint::HasAnnotationMethodConstraint(
    const std::string& type,
    const std::optional<std::string>& annotation)
    : type_(std::move(type)), annotation_(std::move(annotation)) {}

bool HasAnnotationMethodConstraint::satisfy(const Method* method) const {
  return has_annotation(
      method->dex_method()->get_anno_set(), type_, annotation_);
}

bool HasAnnotationMethodConstraint::operator==(
    const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const HasAnnotationMethodConstraint*>(&other)) {
    return other_constraint->type_ == type_ &&
        ((!other_constraint->annotation_ && !annotation_) ||
         (other_constraint->annotation_ && annotation_ &&
          other_constraint->annotation_->pattern() == annotation_->pattern()));
  } else {
    return false;
  }
}

ParameterConstraint::ParameterConstraint(
    ParameterPosition index,
    std::unique_ptr<TypeConstraint> inner_constraint)
    : index_(index), inner_constraint_(std::move(inner_constraint)) {}

bool ParameterConstraint::satisfy(const Method* method) const {
  const auto type = method->parameter_type(index_);
  return type ? inner_constraint_->satisfy(type) : false;
}

bool ParameterConstraint::operator==(const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const ParameterConstraint*>(&other)) {
    return other_constraint->index_ == index_ &&
        *(other_constraint->inner_constraint_) == *inner_constraint_;
  } else {
    return false;
  }
}

SignatureConstraint::SignatureConstraint(const std::string& regex_string)
    : pattern_(regex_string) {}

MethodHashedSet SignatureConstraint::may_satisfy(
    const MethodMappings& method_mappings) const {
  auto string_pattern = as_string_literal(pattern_);
  if (!string_pattern) {
    return MethodHashedSet::top();
  }
  return method_mappings.signature_to_methods.get(
      *string_pattern, MethodHashedSet::bottom());
}

bool SignatureConstraint::satisfy(const Method* method) const {
  return re2::RE2::FullMatch(method->signature(), pattern_);
}

bool SignatureConstraint::operator==(const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const SignatureConstraint*>(&other)) {
    return other_constraint->pattern_.pattern() == pattern_.pattern();
  } else {
    return false;
  }
}

ReturnConstraint::ReturnConstraint(
    std::unique_ptr<TypeConstraint> inner_constraint)
    : inner_constraint_(std::move(inner_constraint)) {}

bool ReturnConstraint::satisfy(const Method* method) const {
  return inner_constraint_->satisfy(method->get_proto()->get_rtype());
}

bool ReturnConstraint::operator==(const MethodConstraint& other) const {
  if (auto* other_constraint = dynamic_cast<const ReturnConstraint*>(&other)) {
    return *(other_constraint->inner_constraint_) == *inner_constraint_;
  } else {
    return false;
  }
}

VisibilityMethodConstraint::VisibilityMethodConstraint(
    DexAccessFlags visibility)
    : visibility_(visibility) {}

bool VisibilityMethodConstraint::satisfy(const Method* method) const {
  return method->get_access() & visibility_;
}

bool VisibilityMethodConstraint::operator==(
    const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const VisibilityMethodConstraint*>(&other)) {
    return other_constraint->visibility_ == visibility_;
  } else {
    return false;
  }
}

bool MethodConstraint::has_children() const {
  return false;
}

std::vector<const MethodConstraint*> MethodConstraint::children() const {
  return {};
}

MethodHashedSet MethodConstraint::may_satisfy(
    const MethodMappings& /* method_mappings */) const {
  return MethodHashedSet::top();
}

namespace {

std::optional<DexAccessFlags> string_to_visibility(
    const std::string& visibility) {
  if (visibility == "public") {
    return ACC_PUBLIC;
  } else if (visibility == "private") {
    return ACC_PRIVATE;
  } else if (visibility == "protected") {
    return ACC_PROTECTED;
  } else {
    return std::nullopt;
  }
}

} // namespace

std::unique_ptr<MethodConstraint> MethodConstraint::from_json(
    const Json::Value& constraint,
    Context& context) {
  JsonValidation::validate_object(constraint);

  std::string constraint_name =
      JsonValidation::string(constraint, "constraint");
  if (constraint_name == "name") {
    return std::make_unique<MethodNameConstraint>(
        JsonValidation::string(constraint, "pattern"));
  } else if (constraint_name == "parent") {
    return std::make_unique<ParentConstraint>(TypeConstraint::from_json(
        JsonValidation::object(constraint, /* field */ "inner")));
  } else if (constraint_name == "number_parameters") {
    return std::make_unique<NumberParametersConstraint>(
        IntegerConstraint::from_json(
            JsonValidation::object(constraint, /* field */ "inner")));
  } else if (constraint_name == "number_overrides") {
    return std::make_unique<NumberOverridesConstraint>(
        IntegerConstraint::from_json(
            JsonValidation::object(constraint, /* field */ "inner")),
        context);
  } else if (constraint_name == "is_static") {
    bool expected = constraint.isMember("value")
        ? JsonValidation::boolean(constraint, /* field */ "value")
        : true;
    return std::make_unique<IsStaticConstraint>(expected);
  } else if (constraint_name == "is_constructor") {
    bool expected = constraint.isMember("value")
        ? JsonValidation::boolean(constraint, /* field */ "value")
        : true;
    return std::make_unique<IsConstructorConstraint>(expected);
  } else if (constraint_name == "is_native") {
    bool expected = constraint.isMember("value")
        ? JsonValidation::boolean(constraint, /* field */ "value")
        : true;
    return std::make_unique<IsNativeConstraint>(expected);
  } else if (constraint_name == "parameter") {
    int index = JsonValidation::integer(constraint, /* field */ "idx");
    return std::make_unique<ParameterConstraint>(
        index,
        TypeConstraint::from_json(
            JsonValidation::object(constraint, /* field */ "inner")));
  } else if (constraint_name == "signature") {
    return std::make_unique<SignatureConstraint>(
        JsonValidation::string(constraint, /* field */ "pattern"));
  } else if (constraint_name == "any_of" || constraint_name == "all_of") {
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    for (const auto& inner :
         JsonValidation::null_or_array(constraint, /* field */ "inners")) {
      constraints.push_back(MethodConstraint::from_json(inner, context));
    }
    if (constraint_name == "any_of") {
      return std::make_unique<AnyOfMethodConstraint>(std::move(constraints));
    } else {
      return std::make_unique<AllOfMethodConstraint>(std::move(constraints));
    }
  } else if (constraint_name == "return") {
    return std::make_unique<ReturnConstraint>(TypeConstraint::from_json(
        JsonValidation::object(constraint, /* field */ "inner")));
  } else if (constraint_name == "visibility") {
    auto visibility_string =
        JsonValidation::string(constraint, /* field */ "is");
    auto visibility = string_to_visibility(visibility_string);
    if (!visibility) {
      throw JsonValidationError(
          constraint,
          /* field */ "is",
          /* expected */ "`public`, `private` or `protected`");
    }
    return std::make_unique<VisibilityMethodConstraint>(*visibility);
  } else if (constraint_name == "not") {
    return std::make_unique<NotMethodConstraint>(MethodConstraint::from_json(
        JsonValidation::object(constraint, /* field */ "inner"), context));
  } else if (constraint_name == "has_code") {
    bool expected = constraint.isMember("value")
        ? JsonValidation::boolean(constraint, /* field */ "value")
        : true;
    return std::make_unique<HasCodeConstraint>(expected);
  } else if (constraint_name == "has_annotation") {
    return std::make_unique<HasAnnotationMethodConstraint>(
        JsonValidation::string(constraint, "type"),
        constraint.isMember("pattern")
            ? std::optional<std::string>{JsonValidation::string(
                  constraint, "pattern")}
            : std::nullopt);
  } else {
    throw JsonValidationError(
        constraint,
        /* field */ "constraint",
        /* expected */ "valid constraint type");
    return nullptr;
  }
}

} // namespace marianatrench

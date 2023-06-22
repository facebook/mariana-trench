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
#include <mariana-trench/constraints/MethodConstraints.h>
#include <mariana-trench/constraints/TypeConstraints.h>

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

MethodPatternConstraint::MethodPatternConstraint(
    const std::string& regex_string)
    : pattern_(regex_string) {}

MethodHashedSet MethodPatternConstraint::may_satisfy(
    const MethodMappings& method_mappings) const {
  auto string_pattern = as_string_literal(pattern_);
  if (!string_pattern) {
    return MethodHashedSet::top();
  }
  return method_mappings.name_to_methods().get(
      *string_pattern, MethodHashedSet::bottom());
}

bool MethodPatternConstraint::satisfy(const Method* method) const {
  return re2::RE2::FullMatch(method->get_name(), pattern_);
}

bool MethodPatternConstraint::operator==(const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const MethodPatternConstraint*>(&other)) {
    return other_constraint->pattern_.pattern() == pattern_.pattern();
  } else {
    return false;
  }
}

MethodNameConstraint::MethodNameConstraint(std::string name)
    : name_(std::move(name)) {}

MethodHashedSet MethodNameConstraint::may_satisfy(
    const MethodMappings& method_mappings) const {
  return method_mappings.name_to_methods().get(
      name_, MethodHashedSet::bottom());
}

bool MethodNameConstraint::satisfy(const Method* method) const {
  return method->get_name() == name_;
}

bool MethodNameConstraint::operator==(const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const MethodNameConstraint*>(&other)) {
    return other_constraint->name_ == name_;
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
  MethodHashedSet all_methods = method_mappings.all_methods();
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
    std::string type,
    std::optional<std::string> annotation)
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

MethodHashedSet HasAnnotationMethodConstraint::may_satisfy(
    const MethodMappings& method_mappings) const {
  return method_mappings.annotation_type_to_methods().get(
      type_, MethodHashedSet::bottom());
}

NthParameterConstraint::NthParameterConstraint(
    ParameterPosition index,
    std::unique_ptr<ParameterConstraint> inner_constraint)
    : index_(index), inner_constraint_(std::move(inner_constraint)) {}

bool NthParameterConstraint::satisfy(const Method* method) const {
  const auto* type = method->parameter_type(index_);
  const auto* annotations_set = method->get_parameter_annotations(index_);

  return type ? inner_constraint_->satisfy(annotations_set, type) : false;
}

bool NthParameterConstraint::operator==(const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const NthParameterConstraint*>(&other)) {
    return other_constraint->index_ == index_ &&
        *(other_constraint->inner_constraint_) == *inner_constraint_;
  } else {
    return false;
  }
}

AnyParameterConstraint::AnyParameterConstraint(
    std::optional<ParameterPosition> start_index,
    std::unique_ptr<ParameterConstraint> inner_constraint)
    : start_index_(start_index),
      inner_constraint_(std::move(inner_constraint)) {}

bool AnyParameterConstraint::satisfy(const Method* method) const {
  ParameterPosition i;
  // always exclude `this`
  for (i = start_index_.value_or(0) + method->first_parameter_index();
       i < method->number_of_parameters();
       i++) {
    const auto* type = method->parameter_type(i);
    const auto* annotations_set = method->get_parameter_annotations(i);
    if (type != nullptr && inner_constraint_->satisfy(annotations_set, type)) {
      return true;
    }
  }
  return false;
}

bool AnyParameterConstraint::operator==(const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const AnyParameterConstraint*>(&other)) {
    return other_constraint->start_index_ == start_index_ &&
        *(other_constraint->inner_constraint_) == *inner_constraint_;
  } else {
    return false;
  }
}

SignaturePatternConstraint::SignaturePatternConstraint(
    const std::string& regex_string)
    : pattern_(regex_string) {}

MethodHashedSet SignaturePatternConstraint::may_satisfy(
    const MethodMappings& method_mappings) const {
  auto string_pattern = as_string_literal(pattern_);
  if (!string_pattern) {
    return MethodHashedSet::top();
  }
  return method_mappings.signature_to_methods().get(
      *string_pattern, MethodHashedSet::bottom());
}

bool SignaturePatternConstraint::satisfy(const Method* method) const {
  return re2::RE2::FullMatch(method->signature(), pattern_);
}

bool SignaturePatternConstraint::operator==(
    const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const SignaturePatternConstraint*>(&other)) {
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

MethodHasStringConstraint::MethodHasStringConstraint(
    const std::string& regex_string)
    : pattern_(regex_string) {}

bool MethodHasStringConstraint::satisfy(const Method* method) const {
  const auto& code = method->get_code();
  if (!code) {
    return false;
  }
  const auto& cfg = code->cfg();

  for (const auto* block : cfg.blocks()) {
    auto show_block = show(block);
    if (re2::RE2::PartialMatch(show_block, pattern_)) {
      return true;
    }
  }
  return false;
}

bool MethodHasStringConstraint::operator==(
    const MethodConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const MethodHasStringConstraint*>(&other)) {
    return other_constraint->pattern_.pattern() == pattern_.pattern();
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

std::unique_ptr<MethodConstraint> signature_match_name_constraint(
    std::string name) {
  return std::make_unique<MethodNameConstraint>(name);
}

std::unique_ptr<MethodConstraint> signature_match_name_constraint(
    const Json::Value& names) {
  std::vector<std::unique_ptr<MethodConstraint>> method_name_constraints;
  for (const auto& name : names) {
    method_name_constraints.push_back(
        signature_match_name_constraint(JsonValidation::string(name)));
  }
  return std::make_unique<AnyOfMethodConstraint>(
      std::move(method_name_constraints));
}

std::unique_ptr<MethodConstraint> signature_match_parent_constraint(
    std::string parent) {
  return std::make_unique<ParentConstraint>(
      std::make_unique<TypeNameConstraint>(parent));
}

std::unique_ptr<MethodConstraint> signature_match_parent_constraint(
    const Json::Value& parents) {
  std::vector<std::unique_ptr<MethodConstraint>> parent_name_constraints;
  for (const auto& parent : parents) {
    parent_name_constraints.push_back(
        signature_match_parent_constraint(JsonValidation::string(parent)));
  }
  return std::make_unique<AnyOfMethodConstraint>(
      std::move(parent_name_constraints));
}

std::unique_ptr<MethodConstraint> signature_match_parent_extends_constraint(
    std::string parent,
    bool includes_self) {
  return std::make_unique<ParentConstraint>(std::make_unique<ExtendsConstraint>(
      std::make_unique<TypeNameConstraint>(parent), includes_self));
}

std::unique_ptr<MethodConstraint> signature_match_parent_extends_constraint(
    const Json::Value& parents,
    bool includes_self) {
  std::vector<std::unique_ptr<MethodConstraint>> parent_extends_constraints;
  for (const auto& parent : parents) {
    parent_extends_constraints.push_back(
        signature_match_parent_extends_constraint(
            JsonValidation::string(parent), includes_self));
  }
  return std::make_unique<AnyOfMethodConstraint>(
      std::move(parent_extends_constraints));
}

} // namespace

std::unique_ptr<MethodConstraint> MethodConstraint::from_json(
    const Json::Value& constraint,
    Context& context) {
  JsonValidation::validate_object(constraint);

  std::string constraint_name =
      JsonValidation::string(constraint, "constraint");
  if (constraint_name == "name") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "pattern"});
    return std::make_unique<MethodPatternConstraint>(
        JsonValidation::string(constraint, "pattern"));
  } else if (constraint_name == "parent") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "inner", "pattern"});
    if (constraint.isMember("inner") && constraint.isMember("pattern")) {
      throw JsonValidationError(
          constraint,
          std::nullopt,
          "parent constraints may only have one of `inner` and `pattern`.");
    }
    if (constraint.isMember("inner")) {
      return std::make_unique<ParentConstraint>(TypeConstraint::from_json(
          JsonValidation::object(constraint, /* field */ "inner")));
    } else {
      if (!constraint.isMember("pattern")) {
        throw JsonValidationError(
            constraint,
            std::nullopt,
            "parent constraints must have one of `inner` and `pattern` as a field.");
      }
      return std::make_unique<ParentConstraint>(
          std::make_unique<TypePatternConstraint>(
              JsonValidation::string(constraint, /* field */ "pattern")));
    }
  } else if (constraint_name == "number_parameters") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "inner"});
    return std::make_unique<NumberParametersConstraint>(
        IntegerConstraint::from_json(
            JsonValidation::object(constraint, /* field */ "inner")));
  } else if (constraint_name == "number_overrides") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "inner"});
    return std::make_unique<NumberOverridesConstraint>(
        IntegerConstraint::from_json(
            JsonValidation::object(constraint, /* field */ "inner")),
        context);
  } else if (constraint_name == "is_static") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "value"});
    bool expected = constraint.isMember("value")
        ? JsonValidation::boolean(constraint, /* field */ "value")
        : true;
    return std::make_unique<IsStaticConstraint>(expected);
  } else if (constraint_name == "is_constructor") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "value"});
    bool expected = constraint.isMember("value")
        ? JsonValidation::boolean(constraint, /* field */ "value")
        : true;
    return std::make_unique<IsConstructorConstraint>(expected);
  } else if (constraint_name == "is_native") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "value"});
    bool expected = constraint.isMember("value")
        ? JsonValidation::boolean(constraint, /* field */ "value")
        : true;
    return std::make_unique<IsNativeConstraint>(expected);
  } else if (constraint_name == "parameter") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "idx", "inner"});
    int index = JsonValidation::integer(constraint, /* field */ "idx");
    return std::make_unique<NthParameterConstraint>(
        index,
        ParameterConstraint::from_json(
            JsonValidation::object(constraint, /* field */ "inner")));
  } else if (constraint_name == "any_parameter") {
    JsonValidation::check_unexpected_members(
        constraint, {"start_idx", "constraint", "inner"});
    std::optional<ParameterPosition> index =
        JsonValidation::optional_integer(constraint, /* field */ "start_idx");
    return std::make_unique<AnyParameterConstraint>(
        index,
        ParameterConstraint::from_json(
            JsonValidation::object(constraint, /* field */ "inner")));
  } else if (constraint_name == "signature") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "pattern"});
    return std::make_unique<SignaturePatternConstraint>(
        JsonValidation::string(constraint, /* field */ "pattern"));
  } else if (constraint_name == "signature_pattern") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "pattern"});
    return std::make_unique<SignaturePatternConstraint>(
        JsonValidation::string(constraint, /* field */ "pattern"));
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
    std::vector<std::unique_ptr<MethodConstraint>> constraints;
    int name_count = 0;
    int parent_count = 0;
    if (constraint.isMember("name")) {
      name_count++;
      constraints.push_back(signature_match_name_constraint(
          JsonValidation::string(constraint, /* field */ "name")));
    }
    if (constraint.isMember("names")) {
      name_count++;
      constraints.push_back(signature_match_name_constraint(
          JsonValidation::nonempty_array(constraint, /* field */ "names")));
    }
    if (constraint.isMember("parent")) {
      parent_count++;
      constraints.push_back(signature_match_parent_constraint(
          JsonValidation::string(constraint, /* field */ "parent")));
    }
    if (constraint.isMember("parents")) {
      parent_count++;
      constraints.push_back(signature_match_parent_constraint(
          JsonValidation::nonempty_array(constraint, /* field */ "parents")));
    }
    if (constraint.isMember("extends")) {
      parent_count++;
      bool includes_self = constraint.isMember("include_self")
          ? JsonValidation::boolean(constraint, /* field */ "include_self")
          : true;
      const auto& extends_field = constraint["extends"];
      if (extends_field.isString()) {
        constraints.push_back(signature_match_parent_extends_constraint(
            JsonValidation::string(constraint, /* field */ "extends"),
            includes_self));
      } else {
        constraints.push_back(signature_match_parent_extends_constraint(
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
          /* expected */
          "Exactly one of \"name\" and \"names\" should be present.");
    }
    return std::make_unique<AllOfMethodConstraint>(std::move(constraints));
  } else if (constraint_name == "bytecode") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "pattern"});
    return std::make_unique<MethodHasStringConstraint>(
        JsonValidation::string(constraint, /* field */ "pattern"));
  } else if (constraint_name == "any_of" || constraint_name == "all_of") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "inners"});
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
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "inner"});
    return std::make_unique<ReturnConstraint>(TypeConstraint::from_json(
        JsonValidation::object(constraint, /* field */ "inner")));
  } else if (constraint_name == "visibility") {
    JsonValidation::check_unexpected_members(constraint, {"constraint", "is"});
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
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "inner"});
    return std::make_unique<NotMethodConstraint>(MethodConstraint::from_json(
        JsonValidation::object(constraint, /* field */ "inner"), context));
  } else if (constraint_name == "has_code") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "value"});
    bool expected = constraint.isMember("value")
        ? JsonValidation::boolean(constraint, /* field */ "value")
        : true;
    return std::make_unique<HasCodeConstraint>(expected);
  } else if (constraint_name == "has_annotation") {
    JsonValidation::check_unexpected_members(
        constraint, {"constraint", "type", "pattern"});
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

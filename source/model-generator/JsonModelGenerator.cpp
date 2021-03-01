/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <optional>

#include <json/json.h>

#include <Show.h>
#include <Walkers.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/RE2.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/Timer.h>
#include <mariana-trench/model-generator/JsonModelGenerator.h>

namespace {

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

} // namespace

namespace marianatrench {

MethodSet TypeConstraint::may_satisfy(
    const MethodMappings& /* method_mappings */,
    MaySatisfyMethodConstraintKind /* constraint_kind */) const {
  return MethodSet::top();
}

TypeNameConstraint::TypeNameConstraint(std::string regex_string)
    : pattern_(regex_string) {}

MethodSet TypeNameConstraint::may_satisfy(
    const MethodMappings& method_mappings,
    MaySatisfyMethodConstraintKind constraint_kind) const {
  auto string_pattern = as_string_literal(pattern_);
  if (!string_pattern) {
    return MethodSet::top();
  }
  switch (constraint_kind) {
    case MaySatisfyMethodConstraintKind::Parent:
      return method_mappings.class_to_methods.get(
          *string_pattern, MethodSet::bottom());
    case MaySatisfyMethodConstraintKind::Extends:
      return method_mappings.class_to_override_methods.get(
          *string_pattern, MethodSet::bottom());
  }
}

bool TypeNameConstraint::satisfy(const DexType* type) const {
  return re2::RE2::FullMatch(type->str(), pattern_);
}

bool TypeNameConstraint::operator==(const TypeConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const TypeNameConstraint*>(&other)) {
    return other_constraint->pattern_.pattern() == pattern_.pattern();
  } else {
    return false;
  }
}

HasAnnotationTypeConstraint::HasAnnotationTypeConstraint(
    std::string type,
    std::optional<std::string> annotation)
    : type_(std::move(type)), annotation_(std::move(annotation)) {}

bool HasAnnotationTypeConstraint::satisfy(const DexType* type) const {
  auto* clazz = type_class(type);
  return clazz && has_annotation(clazz->get_anno_set(), type_, annotation_);
}

bool HasAnnotationTypeConstraint::operator==(
    const TypeConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const HasAnnotationTypeConstraint*>(&other)) {
    return other_constraint->type_ == type_ &&
        ((!other_constraint->annotation_ && !annotation_) ||
         (other_constraint->annotation_ && annotation_ &&
          other_constraint->annotation_->pattern() == annotation_->pattern()));
  } else {
    return false;
  }
}

ExtendsConstraint::ExtendsConstraint(
    std::unique_ptr<TypeConstraint> inner_constraint,
    bool include_self)
    : inner_constraint_(std::move(inner_constraint)),
      include_self_(include_self) {}

MethodSet ExtendsConstraint::may_satisfy(
    const MethodMappings& method_mappings,
    MaySatisfyMethodConstraintKind /* constraint_kind */) const {
  return inner_constraint_->may_satisfy(
      method_mappings, MaySatisfyMethodConstraintKind::Extends);
};

bool ExtendsConstraint::satisfy(const DexType* type) const {
  auto* current_type = type;
  do {
    if ((include_self_ || current_type != type) &&
        inner_constraint_->satisfy(current_type)) {
      return true;
    }
    DexClass* klass = type_class(current_type);
    if (!klass) {
      break;
    }
    for (auto* interface : *klass->get_interfaces()) {
      if (inner_constraint_->satisfy(interface)) {
        return true;
      }
    }

    current_type = klass->get_super_class();
  } while (current_type);
  return false;
}

bool ExtendsConstraint::operator==(const TypeConstraint& other) const {
  if (auto* other_constraint = dynamic_cast<const ExtendsConstraint*>(&other)) {
    return *(other_constraint->inner_constraint_) == *inner_constraint_ &&
        other_constraint->include_self_ == include_self_;
  } else {
    return false;
  }
}

SuperConstraint::SuperConstraint(
    std::unique_ptr<TypeConstraint> inner_constraint)
    : inner_constraint_(std::move(inner_constraint)) {}

bool SuperConstraint::satisfy(const DexType* type) const {
  DexClass* klass = type_class(type);
  if (!klass) {
    return false;
  }
  type = klass->get_super_class();
  return type && inner_constraint_->satisfy(type);
}

bool SuperConstraint::operator==(const TypeConstraint& other) const {
  if (auto* other_constraint = dynamic_cast<const SuperConstraint*>(&other)) {
    return *(other_constraint->inner_constraint_) == *inner_constraint_;
  } else {
    return false;
  }
}

IsClassTypeConstraint::IsClassTypeConstraint(bool expected)
    : expected_(expected) {}

bool IsClassTypeConstraint::satisfy(const DexType* type) const {
  static const RE2 class_signature = "L.+;";
  bool is_class = re2::RE2::FullMatch(type->str(), class_signature);
  return expected_ == is_class;
}

bool IsClassTypeConstraint::operator==(const TypeConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const IsClassTypeConstraint*>(&other)) {
    return other_constraint->expected_ == expected_;
  } else {
    return false;
  }
}

IsInterfaceTypeConstraint::IsInterfaceTypeConstraint(bool expected)
    : expected_(expected) {}

bool IsInterfaceTypeConstraint::satisfy(const DexType* type) const {
  DexClass* klass = type_class(type);
  bool is_interface_type = klass != nullptr && is_interface(klass);
  return expected_ == is_interface_type;
}

bool IsInterfaceTypeConstraint::operator==(const TypeConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const IsInterfaceTypeConstraint*>(&other)) {
    return other_constraint->expected_ == expected_;
  } else {
    return false;
  }
}

AllOfTypeConstraint::AllOfTypeConstraint(
    std::vector<std::unique_ptr<TypeConstraint>> constraints)
    : inner_constraints_(std::move(constraints)) {}

MethodSet AllOfTypeConstraint::may_satisfy(
    const MethodMappings& method_mappings,
    MaySatisfyMethodConstraintKind constraint_kind) const {
  MethodSet intersection_set = MethodSet::top();
  for (const auto& constraint : inner_constraints_) {
    intersection_set.meet_with(
        constraint->may_satisfy(method_mappings, constraint_kind));
  }
  return intersection_set;
}

bool AllOfTypeConstraint::satisfy(const DexType* type) const {
  return std::all_of(
      inner_constraints_.begin(),
      inner_constraints_.end(),
      [type](const auto& constraint) { return constraint->satisfy(type); });
}

bool AllOfTypeConstraint::operator==(const TypeConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const AllOfTypeConstraint*>(&other)) {
    return std::is_permutation(
        other_constraint->inner_constraints_.begin(),
        other_constraint->inner_constraints_.end(),
        inner_constraints_.begin(),
        inner_constraints_.end(),
        [](const auto& left, const auto& right) { return *left == *right; });
  } else {
    return false;
  }
}

AnyOfTypeConstraint::AnyOfTypeConstraint(
    std::vector<std::unique_ptr<TypeConstraint>> constraints)
    : inner_constraints_(std::move(constraints)) {}

MethodSet AnyOfTypeConstraint::may_satisfy(
    const MethodMappings& method_mappings,
    MaySatisfyMethodConstraintKind constraint_kind) const {
  if (inner_constraints_.empty()) {
    return MethodSet::top();
  }
  MethodSet union_set = MethodSet::bottom();
  for (const auto& constraint : inner_constraints_) {
    union_set.join_with(
        constraint->may_satisfy(method_mappings, constraint_kind));
  }
  return union_set;
}

bool AnyOfTypeConstraint::satisfy(const DexType* type) const {
  // If there is no constraint, the type vacuously satisfies the constraint
  // This is different from the semantic of std::any_of
  if (inner_constraints_.empty()) {
    return true;
  }
  return std::any_of(
      inner_constraints_.begin(),
      inner_constraints_.end(),
      [type](const auto& constraint) { return constraint->satisfy(type); });
}

bool AnyOfTypeConstraint::operator==(const TypeConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const AnyOfTypeConstraint*>(&other)) {
    return std::is_permutation(
        other_constraint->inner_constraints_.begin(),
        other_constraint->inner_constraints_.end(),
        inner_constraints_.begin(),
        inner_constraints_.end(),
        [](const auto& left, const auto& right) { return *left == *right; });
  } else {
    return false;
  }
}

NotTypeConstraint::NotTypeConstraint(std::unique_ptr<TypeConstraint> constraint)
    : constraint_(std::move(constraint)) {}

MethodSet NotTypeConstraint::may_satisfy(
    const MethodMappings& method_mappings,
    MaySatisfyMethodConstraintKind constraint_kind) const {
  MethodSet child_methods =
      constraint_->may_satisfy(method_mappings, constraint_kind);
  if (child_methods.is_top() || child_methods.is_bottom()) {
    return MethodSet::top();
  }
  MethodSet all_methods = method_mappings.all_methods;
  all_methods.difference_with(child_methods);
  return all_methods;
}

bool NotTypeConstraint::satisfy(const DexType* type) const {
  return !constraint_->satisfy(type);
}

bool NotTypeConstraint::operator==(const TypeConstraint& other) const {
  if (auto* other_constraint = dynamic_cast<const NotTypeConstraint*>(&other)) {
    return *(other_constraint->constraint_) == *constraint_;
  } else {
    return false;
  }
}

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

MethodNameConstraint::MethodNameConstraint(std::string regex_string)
    : pattern_(regex_string) {}

MethodSet MethodNameConstraint::may_satisfy(
    const MethodMappings& method_mappings) const {
  auto string_pattern = as_string_literal(pattern_);
  if (!string_pattern) {
    return MethodSet::top();
  }
  return method_mappings.name_to_methods.get(
      *string_pattern, MethodSet::bottom());
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

MethodSet ParentConstraint::may_satisfy(
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

MethodSet AllOfMethodConstraint::may_satisfy(
    const MethodMappings& method_mappings) const {
  auto intersection_set = MethodSet::top();
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

MethodSet AnyOfMethodConstraint::may_satisfy(
    const MethodMappings& method_mappings) const {
  if (constraints_.empty()) {
    return MethodSet::top();
  }
  auto union_set = MethodSet::bottom();
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

MethodSet NotMethodConstraint::may_satisfy(
    const MethodMappings& method_mappings) const {
  MethodSet child_methods = constraint_->may_satisfy(method_mappings);
  if (child_methods.is_top() || child_methods.is_bottom()) {
    return MethodSet::top();
  }
  MethodSet all_methods = method_mappings.all_methods;
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

SignatureConstraint::SignatureConstraint(std::string regex_string)
    : pattern_(regex_string) {}

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

MethodSet MethodConstraint::may_satisfy(
    const MethodMappings& /* method_mappings */) const {
  return MethodSet::top();
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

std::unique_ptr<TypeConstraint> TypeConstraint::from_json(
    const Json::Value& constraint) {
  JsonValidation::validate_object(constraint);

  std::string constraint_name =
      JsonValidation::string(constraint, "constraint");
  if (constraint_name == "name") {
    return std::make_unique<TypeNameConstraint>(
        JsonValidation::string(constraint, "pattern"));
  } else if (constraint_name == "extends") {
    bool includes_self = constraint.isMember("include_self")
        ? JsonValidation::boolean(constraint, /* field */ "include_self")
        : true;
    return std::make_unique<ExtendsConstraint>(
        TypeConstraint::from_json(
            JsonValidation::object(constraint, /* field */ "inner")),
        includes_self);
  } else if (constraint_name == "super") {
    return std::make_unique<SuperConstraint>(TypeConstraint::from_json(
        JsonValidation::object(constraint, /* field */ "inner")));
  } else if (constraint_name == "not") {
    return std::make_unique<NotTypeConstraint>(TypeConstraint::from_json(
        JsonValidation::object(constraint, /* field */ "inner")));
  } else if (constraint_name == "any_of" || constraint_name == "all_of") {
    std::vector<std::unique_ptr<TypeConstraint>> constraints;
    for (const auto& inner :
         JsonValidation::null_or_array(constraint, /* field */ "inners")) {
      constraints.push_back(TypeConstraint::from_json(inner));
    }
    if (constraint_name == "any_of") {
      return std::make_unique<AnyOfTypeConstraint>(std::move(constraints));
    } else {
      return std::make_unique<AllOfTypeConstraint>(std::move(constraints));
    }
  } else if (constraint_name == "has_annotation") {
    return std::make_unique<HasAnnotationTypeConstraint>(
        JsonValidation::string(constraint, "type"),
        constraint.isMember("pattern")
            ? std::optional<std::string>{JsonValidation::string(
                  constraint, "pattern")}
            : std::nullopt);
  } else if (
      constraint_name == "is_class" || constraint_name == "is_interface") {
    bool expected = constraint.isMember("value")
        ? JsonValidation::boolean(constraint, /* field */ "value")
        : true;
    if (constraint_name == "is_class") {
      return std::make_unique<IsClassTypeConstraint>(expected);
    } else {
      return std::make_unique<IsInterfaceTypeConstraint>(expected);
    }
  } else {
    throw JsonValidationError(
        constraint,
        /* field */ "constraint",
        /* expected */ "valid constraint type");
    return nullptr;
  }
}

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

TemplateVariableMapping::TemplateVariableMapping() {}

void TemplateVariableMapping::insert(
    const std::string& name,
    ParameterPosition index) {
  map_.insert_or_assign(name, index);
}

std::optional<ParameterPosition> TemplateVariableMapping::at(
    const std::string& name) const {
  try {
    return map_.at(name);
  } catch (const std::out_of_range& exception) {
    return std::nullopt;
  }
}

ParameterPositionTemplate::ParameterPositionTemplate(
    ParameterPosition parameter_position)
    : parameter_position_(parameter_position) {}

ParameterPositionTemplate::ParameterPositionTemplate(
    std::string parameter_position)
    : parameter_position_(std::move(parameter_position)) {}

std::string ParameterPositionTemplate::to_string() const {
  if (std::holds_alternative<ParameterPosition>(parameter_position_)) {
    return std::to_string(std::get<ParameterPosition>(parameter_position_));
  } else if (std::holds_alternative<std::string>(parameter_position_)) {
    return std::get<std::string>(parameter_position_);
  } else {
    mt_unreachable();
  }
}

ParameterPosition ParameterPositionTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions) const {
  if (std::holds_alternative<ParameterPosition>(parameter_position_)) {
    return std::get<ParameterPosition>(parameter_position_);
  } else if (std::holds_alternative<std::string>(parameter_position_)) {
    auto result =
        parameter_positions.at(std::get<std::string>(parameter_position_));
    if (result) {
      return *result;
    } else {
      throw JsonValidationError(
          Json::Value(std::get<std::string>(parameter_position_)),
          /* field */ "parameter_position",
          /* expected */ "a variable name that is defined in \"variable\"");
    }
  } else {
    mt_unreachable();
  }
}

RootTemplate::RootTemplate(
    Root::Kind kind,
    std::optional<ParameterPositionTemplate> parameter_position)
    : kind_(kind), parameter_position_(parameter_position) {}

bool RootTemplate::is_argument() const {
  return kind_ == Root::Kind::Argument;
}

std::string RootTemplate::to_string() const {
  return is_argument()
      ? fmt::format("Argument({})", parameter_position_->to_string())
      : "Return";
}

Root RootTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions) const {
  switch (kind_) {
    case Root::Kind::Return: {
      return Root(Root::Kind::Return);
    }
    case Root::Kind::Argument: {
      mt_assert(parameter_position_);
      return Root(
          Root::Kind::Argument,
          parameter_position_->instantiate(parameter_positions));
    }
    default: {
      mt_unreachable();
    }
  }
}

AccessPathTemplate::AccessPathTemplate(RootTemplate root, Path path)
    : root_(root), path_(std::move(path)) {}

AccessPathTemplate AccessPathTemplate::from_json(const Json::Value& value) {
  auto elements = AccessPath::split_path(value);

  if (elements.empty()) {
    throw JsonValidationError(
        value, /* field */ std::nullopt, "non-empty string for access path");
  }

  // Parse the root.
  auto root = RootTemplate(Root::Kind::Return);
  const auto& root_string = elements.front();

  if (boost::starts_with(root_string, "Argument(") &&
      boost::ends_with(root_string, ")") && root_string.size() >= 11) {
    auto parameter_string = root_string.substr(9, root_string.size() - 10);
    auto parameter = parse_parameter_position(parameter_string);
    if (parameter) {
      root = RootTemplate(
          Root::Kind::Argument, ParameterPositionTemplate(*parameter));
    } else {
      root = RootTemplate(
          Root::Kind::Argument, ParameterPositionTemplate(parameter_string));
    }
  } else if (root_string != "Return") {
    throw JsonValidationError(
        value,
        /* field */ std::nullopt,
        fmt::format(
            "valid access path root (`Return` or `Argument(...)`), got `{}`",
            root_string));
  }

  Path path;
  for (auto iterator = std::next(elements.begin()), end = elements.end();
       iterator != end;
       ++iterator) {
    path.append(DexString::make_string(*iterator));
  }

  return AccessPathTemplate(root, path);
}

Json::Value AccessPathTemplate::to_json() const {
  std::string value;

  value.append(root_.to_string());
  for (auto* field : path_) {
    value.append(".");
    value.append(show(field));
  }

  return value;
}

AccessPath AccessPathTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions) const {
  return AccessPath(root_.instantiate(parameter_positions), path_);
}

PropagationTemplate::PropagationTemplate(
    AccessPathTemplate input,
    AccessPathTemplate output,
    FeatureMayAlwaysSet inferred_features,
    FeatureSet user_features)
    : input_(std::move(input)),
      output_(std::move(output)),
      inferred_features_(std::move(inferred_features)),
      user_features_(std::move(user_features)) {}

PropagationTemplate PropagationTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::string(value, /* field */ "input");
  auto input = AccessPathTemplate::from_json(value["input"]);

  if (!input.root().is_argument()) {
    throw JsonValidationError(
        value, /* field */ "input", "an access path to an argument");
  }

  JsonValidation::string(value, /* field */ "output");
  auto output = AccessPathTemplate::from_json(value["output"]);

  auto inferred_features = FeatureMayAlwaysSet::from_json(value, context);
  auto user_features = FeatureSet::from_json(value["features"], context);

  return PropagationTemplate(input, output, inferred_features, user_features);
}

void PropagationTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_propagation(
      Propagation(
          input_.instantiate(parameter_positions),
          inferred_features_,
          user_features_),
      output_.instantiate(parameter_positions));
}

SinkTemplate::SinkTemplate(Frame sink, AccessPathTemplate port)
    : sink_(std::move(sink)), port_(std::move(port)) {}

SinkTemplate SinkTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::string(value, /* field */ "port");
  return SinkTemplate(
      Frame::from_json(value, context),
      AccessPathTemplate::from_json(value["port"]));
}

void SinkTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_sink(port_.instantiate(parameter_positions), sink_);
}

ParameterSourceTemplate::ParameterSourceTemplate(
    Frame source,
    AccessPathTemplate port)
    : source_(std::move(source)), port_(std::move(port)) {}

ParameterSourceTemplate ParameterSourceTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::string(value, /* field */ "port");
  return ParameterSourceTemplate(
      Frame::from_json(value, context),
      AccessPathTemplate::from_json(value["port"]));
}

void ParameterSourceTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_parameter_source(port_.instantiate(parameter_positions), source_);
}

GenerationTemplate::GenerationTemplate(Frame source, AccessPathTemplate port)
    : source_(std::move(source)), port_(std::move(port)) {}

GenerationTemplate GenerationTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::string(value, /* field */ "port");
  return GenerationTemplate(
      Frame::from_json(value, context),
      AccessPathTemplate::from_json(value["port"]));
}

void GenerationTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_generation(port_.instantiate(parameter_positions), source_);
}

SourceTemplate::SourceTemplate(Frame source, AccessPathTemplate port)
    : source_(std::move(source)), port_(std::move(port)) {}

SourceTemplate SourceTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::string(value, /* field */ "port");
  return SourceTemplate(
      Frame::from_json(value, context),
      AccessPathTemplate::from_json(value["port"]));
}

void SourceTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  if (port_.root().is_argument()) {
    model.add_parameter_source(port_.instantiate(parameter_positions), source_);
  } else {
    model.add_generation(port_.instantiate(parameter_positions), source_);
  }
}

AttachToSourcesTemplate::AttachToSourcesTemplate(
    FeatureSet features,
    RootTemplate port)
    : features_(std::move(features)), port_(std::move(port)) {}

AttachToSourcesTemplate AttachToSourcesTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::null_or_array(value, /* field */ "features");
  auto features = FeatureSet::from_json(value["features"], context);

  JsonValidation::string(value, /* field */ "port");
  auto port = AccessPathTemplate::from_json(value["port"]);

  if (!port.path().empty()) {
    throw JsonValidationError(
        value,
        /* field */ "port",
        /* expected */ "an access path root without field");
  }

  return AttachToSourcesTemplate(features, port.root());
}

void AttachToSourcesTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_attach_to_sources(
      port_.instantiate(parameter_positions), features_);
}

AttachToSinksTemplate::AttachToSinksTemplate(
    FeatureSet features,
    RootTemplate port)
    : features_(std::move(features)), port_(std::move(port)) {}

AttachToSinksTemplate AttachToSinksTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::null_or_array(value, /* field */ "features");
  auto features = FeatureSet::from_json(value["features"], context);

  JsonValidation::string(value, /* field */ "port");
  auto port = AccessPathTemplate::from_json(value["port"]);

  if (!port.path().empty()) {
    throw JsonValidationError(
        value,
        /* field */ "port",
        /* expected */ "an access path root without field");
  }

  return AttachToSinksTemplate(features, port.root());
}

void AttachToSinksTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_attach_to_sinks(port_.instantiate(parameter_positions), features_);
}

AttachToPropagationsTemplate::AttachToPropagationsTemplate(
    FeatureSet features,
    RootTemplate port)
    : features_(std::move(features)), port_(std::move(port)) {}

AttachToPropagationsTemplate AttachToPropagationsTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::null_or_array(value, /* field */ "features");
  auto features = FeatureSet::from_json(value["features"], context);

  JsonValidation::string(value, /* field */ "port");
  auto port = AccessPathTemplate::from_json(value["port"]);

  if (!port.path().empty()) {
    throw JsonValidationError(
        value,
        /* field */ "port",
        /* expected */ "an access path root without field");
  }

  return AttachToPropagationsTemplate(features, port.root());
}

void AttachToPropagationsTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_attach_to_propagations(
      port_.instantiate(parameter_positions), features_);
}

AddFeaturesToArgumentsTemplate::AddFeaturesToArgumentsTemplate(
    FeatureSet features,
    RootTemplate port)
    : features_(std::move(features)), port_(std::move(port)) {}

AddFeaturesToArgumentsTemplate AddFeaturesToArgumentsTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::null_or_array(value, /* field */ "features");
  auto features = FeatureSet::from_json(value["features"], context);

  JsonValidation::string(value, /* field */ "port");
  auto port = AccessPathTemplate::from_json(value["port"]);

  if (!port.path().empty()) {
    throw JsonValidationError(
        value,
        /* field */ "port",
        /* expected */ "an access path root without field");
  }

  return AddFeaturesToArgumentsTemplate(features, port.root());
}

void AddFeaturesToArgumentsTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_add_features_to_arguments(
      port_.instantiate(parameter_positions), features_);
}

ForAllParameters::ForAllParameters(
    std::unique_ptr<AllOfTypeConstraint> constraints,
    std::string variable,
    std::vector<SinkTemplate> sink_templates,
    std::vector<ParameterSourceTemplate> parameter_source_templates,
    std::vector<GenerationTemplate> generation_templates,
    std::vector<SourceTemplate> source_templates,
    std::vector<PropagationTemplate> propagation_templates,
    std::vector<AttachToSourcesTemplate> attach_to_sources_templates,
    std::vector<AttachToSinksTemplate> attach_to_sinks_templates,
    std::vector<AttachToPropagationsTemplate> attach_to_propagations_templates,
    std::vector<AddFeaturesToArgumentsTemplate>
        add_features_to_arguments_templates)
    : constraints_(std::move(constraints)),
      variable_(std::move(variable)),
      sink_templates_(std::move(sink_templates)),
      parameter_source_templates_(std::move(parameter_source_templates)),
      generation_templates_(std::move(generation_templates)),
      source_templates_(std::move(source_templates)),
      propagation_templates_(std::move(propagation_templates)),
      attach_to_sources_templates_(std::move(attach_to_sources_templates)),
      attach_to_sinks_templates_(std::move(attach_to_sinks_templates)),
      attach_to_propagations_templates_(
          std::move(attach_to_propagations_templates)),
      add_features_to_arguments_templates_(
          std::move(add_features_to_arguments_templates)) {}

ForAllParameters ForAllParameters::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  std::vector<std::unique_ptr<TypeConstraint>> constraints;
  std::vector<SinkTemplate> sink_templates;
  std::vector<ParameterSourceTemplate> parameter_source_templates;
  std::vector<GenerationTemplate> generation_templates;
  std::vector<SourceTemplate> source_templates;
  std::vector<PropagationTemplate> propagation_templates;
  std::vector<AttachToSourcesTemplate> attach_to_sources_templates;
  std::vector<AttachToSinksTemplate> attach_to_sinks_templates;
  std::vector<AttachToPropagationsTemplate> attach_to_propagations_templates;
  std::vector<AddFeaturesToArgumentsTemplate>
      add_features_to_arguments_templates;

  std::string variable = JsonValidation::string(value, "variable");

  for (auto constraint :
       JsonValidation::null_or_array(value, /* field */ "where")) {
    // We assume constraints on parameters are type constraints
    constraints.push_back(TypeConstraint::from_json(constraint));
  }

  for (auto sink_value :
       JsonValidation::null_or_array(value, /* field */ "sinks")) {
    sink_templates.push_back(SinkTemplate::from_json(sink_value, context));
  }

  for (auto source_value :
       JsonValidation::null_or_array(value, /* field */ "parameter_sources")) {
    parameter_source_templates.push_back(
        ParameterSourceTemplate::from_json(source_value, context));
  }

  for (auto source_value :
       JsonValidation::null_or_array(value, /* field */ "generations")) {
    generation_templates.push_back(
        GenerationTemplate::from_json(source_value, context));
  }

  for (auto source_value :
       JsonValidation::null_or_array(value, /* field */ "sources")) {
    source_templates.push_back(
        SourceTemplate::from_json(source_value, context));
  }

  for (auto propagation_value :
       JsonValidation::null_or_array(value, /* field */ "propagation")) {
    propagation_templates.push_back(
        PropagationTemplate::from_json(propagation_value, context));
  }

  for (auto attach_to_sources_value :
       JsonValidation::null_or_array(value, /* field */ "attach_to_sources")) {
    attach_to_sources_templates.push_back(
        AttachToSourcesTemplate::from_json(attach_to_sources_value, context));
  }

  for (auto attach_to_sinks_value :
       JsonValidation::null_or_array(value, /* field */ "attach_to_sinks")) {
    attach_to_sinks_templates.push_back(
        AttachToSinksTemplate::from_json(attach_to_sinks_value, context));
  }

  for (auto attach_to_propagations_value : JsonValidation::null_or_array(
           value, /* field */ "attach_to_propagations")) {
    attach_to_propagations_templates.push_back(
        AttachToPropagationsTemplate::from_json(
            attach_to_propagations_value, context));
  }

  for (auto add_features_to_arguments_value : JsonValidation::null_or_array(
           value, /* field */ "add_features_to_arguments")) {
    add_features_to_arguments_templates.push_back(
        AddFeaturesToArgumentsTemplate::from_json(
            add_features_to_arguments_value, context));
  }

  return ForAllParameters(
      std::make_unique<AllOfTypeConstraint>(std::move(constraints)),
      variable,
      sink_templates,
      parameter_source_templates,
      generation_templates,
      source_templates,
      propagation_templates,
      attach_to_sources_templates,
      attach_to_sinks_templates,
      attach_to_propagations_templates,
      add_features_to_arguments_templates);
}

bool ForAllParameters::instantiate(Model& model, const Method* method) const {
  bool updated = false;
  ParameterPosition index = method->first_parameter_index();
  for (auto type : *method->get_proto()->get_args()) {
    if (constraints_->satisfy(type)) {
      LOG(3, "Type {} satifies constraints in for_all_parameters", show(type));
      TemplateVariableMapping variable_mapping;
      variable_mapping.insert(variable_, index);

      for (const auto& sink_template : sink_templates_) {
        sink_template.instantiate(variable_mapping, model);
        updated = true;
      }
      for (const auto& parameter_source_template :
           parameter_source_templates_) {
        parameter_source_template.instantiate(variable_mapping, model);
        updated = true;
      }
      for (const auto& generation_template : generation_templates_) {
        generation_template.instantiate(variable_mapping, model);
        updated = true;
      }
      for (const auto& source_template : source_templates_) {
        source_template.instantiate(variable_mapping, model);
        updated = true;
      }
      for (const auto& propagation_template : propagation_templates_) {
        propagation_template.instantiate(variable_mapping, model);
        updated = true;
      }
      for (const auto& attach_to_sources_template :
           attach_to_sources_templates_) {
        attach_to_sources_template.instantiate(variable_mapping, model);
        updated = true;
      }
      for (const auto& attach_to_sinks_template : attach_to_sinks_templates_) {
        attach_to_sinks_template.instantiate(variable_mapping, model);
        updated = true;
      }
      for (const auto& attach_to_propagations_template :
           attach_to_propagations_templates_) {
        attach_to_propagations_template.instantiate(variable_mapping, model);
        updated = true;
      }
      for (const auto& add_features_to_arguments_template :
           add_features_to_arguments_templates_) {
        add_features_to_arguments_template.instantiate(variable_mapping, model);
        updated = true;
      }
    }
    index++;
  }
  return updated;
}

ModelTemplate::ModelTemplate(
    const Model& model,
    std::vector<ForAllParameters> for_all_parameters)
    : model_(model), for_all_parameters_(std::move(for_all_parameters)) {
  mt_assert(model.method() == nullptr);
}

std::optional<Model> ModelTemplate::instantiate(
    Context& context,
    const Method* method) const {
  Model model = model_.instantiate(method, context);

  bool updated = false;
  for (const auto& for_all_parameters : for_all_parameters_) {
    bool result = for_all_parameters.instantiate(model, method);
    updated = updated || result;
  }

  // An instantiated model can be nonempty even when it is instantiated from
  // an empty model and no new sinks/generations/propagations/sources were
  // introduced by for_all_parameters, because it is possible that a model has
  // non-zero propagations after instantiation (because Model constructor may
  // add default propagations)
  if (!model_.empty() || updated) {
    return model;
  } else {
    LOG(3,
        "Method {} generates no new sinks/generations/propagations/sources from {} for_all_parameters constraints:\nInstantiated model: {}.\nModel template: {}.",
        method->show(),
        for_all_parameters_.size(),
        JsonValidation::to_styled_string(model.to_json()),
        JsonValidation::to_styled_string(model_.to_json()));
    return std::nullopt;
  }
}

ModelTemplate ModelTemplate::from_json(
    const Json::Value& model,
    Context& context) {
  JsonValidation::validate_object(model);

  std::vector<ForAllParameters> for_all_parameters;
  for (auto const& value :
       JsonValidation::null_or_array(model, /* field */ "for_all_parameters")) {
    for_all_parameters.push_back(ForAllParameters::from_json(value, context));
  }

  return ModelTemplate(
      Model::from_json(
          /* method */ nullptr, model, context),
      std::move(for_all_parameters));
}

JsonModelGeneratorItem::JsonModelGeneratorItem(
    const std::string& name,
    Context& context,
    std::unique_ptr<AllOfMethodConstraint> constraint,
    ModelTemplate model_template,
    int verbosity)
    : MethodVisitorModelGenerator(name, context),
      constraint_(std::move(constraint)),
      model_template_(std::move(model_template)),
      verbosity_(verbosity) {}

std::vector<Model> JsonModelGeneratorItem::run_filtered(
    const MethodSet& methods) {
  return this->run_impl(methods.begin(), methods.end());
}

std::vector<Model> JsonModelGeneratorItem::visit_method(
    const Method* method) const {
  std::vector<Model> models;
  if (constraint_->satisfy(method)) {
    LOG(verbosity_,
        "Method `{}{}` satisfies all constraints in json model generator {}",
        method->is_static() ? "(static) " : "",
        method->show(),
        name_);
    auto model = model_template_.instantiate(context_, method);
    // If a method has an empty model, then don't add a model
    if (model) {
      models.push_back(*model);
    }
  }
  return models;
}

std::unordered_set<const MethodConstraint*>
JsonModelGeneratorItem::constraint_leaves() const {
  std::unordered_set<const MethodConstraint*> flattened_constraints;
  std::vector<const MethodConstraint*> constraints = constraint_->children();
  while (!constraints.empty()) {
    const MethodConstraint* constraint = constraints.back();
    constraints.pop_back();
    if (constraint->has_children()) {
      const std::vector<const MethodConstraint*> children =
          constraint->children();
      for (const auto* child : children) {
        constraints.push_back(child);
      }
    } else {
      flattened_constraints.insert(constraint);
    }
  }
  return flattened_constraints;
}

MethodSet JsonModelGeneratorItem::may_satisfy(
    const MethodMappings& method_mappings) const {
  return constraint_->may_satisfy(method_mappings);
}

JsonModelGenerator::JsonModelGenerator(
    const std::string& name,
    Context& context,
    const boost::filesystem::path& json_configuration_file)
    : ModelGenerator(name, context),
      json_configuration_file_(json_configuration_file) {
  const Json::Value& value =
      JsonValidation::parse_json_file(json_configuration_file);
  JsonValidation::validate_object(value);

  for (auto model_generator :
       JsonValidation::null_or_array(value, /* field */ "model_generators")) {
    std::vector<std::unique_ptr<MethodConstraint>> model_constraints;
    int verbosity = model_generator.isMember("verbosity")
        ? JsonValidation::integer(model_generator, "verbosity")
        : 5; // default verbosity

    std::string find_name = JsonValidation::string(model_generator, "find");
    if (find_name == "methods") {
      for (auto constraint : JsonValidation::null_or_array(
               model_generator, /* field */ "where")) {
        model_constraints.push_back(
            MethodConstraint::from_json(constraint, context));
      }
    } else {
      ERROR(1, "Models for `{}` are not supported.", find_name);
    }
    items_.push_back(JsonModelGeneratorItem(
        name,
        context,
        std::make_unique<AllOfMethodConstraint>(std::move(model_constraints)),
        ModelTemplate::from_json(
            JsonValidation::object(model_generator, /* field */ "model"),
            context),
        verbosity));
  }
}

std::vector<Model> JsonModelGenerator::run(const Methods& methods) {
  std::vector<Model> models;
  for (auto& item : items_) {
    std::vector<Model> method_models = item.run(methods);
    models.insert(
        models.end(),
        std::make_move_iterator(method_models.begin()),
        std::make_move_iterator(method_models.end()));
  }
  return models;
}

std::vector<Model> JsonModelGenerator::run_optimized(
    const Methods& methods,
    const MethodMappings& method_mappings) {
  std::vector<Model> models;
  for (auto& item : items_) {
    MethodSet filtered_methods = item.may_satisfy(method_mappings);
    if (filtered_methods.is_bottom()) {
      continue;
    }
    std::vector<Model> method_models;
    if (filtered_methods.is_top()) {
      method_models = item.run(methods);
    } else {
      method_models = item.run_filtered(filtered_methods);
    }
    models.insert(
        models.end(),
        std::make_move_iterator(method_models.begin()),
        std::make_move_iterator(method_models.end()));
  }
  return models;
}

} // namespace marianatrench

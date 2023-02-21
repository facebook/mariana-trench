/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/RE2.h>
#include <mariana-trench/constraints/MethodConstraints.h>
#include <mariana-trench/constraints/TypeConstraints.h>

namespace marianatrench {

MethodHashedSet TypeConstraint::may_satisfy(
    const MethodMappings& /* method_mappings */,
    MaySatisfyMethodConstraintKind /* constraint_kind */) const {
  return MethodHashedSet::top();
}

TypePatternConstraint::TypePatternConstraint(const std::string& regex_string)
    : pattern_(regex_string) {}

MethodHashedSet TypePatternConstraint::may_satisfy(
    const MethodMappings& method_mappings,
    MaySatisfyMethodConstraintKind constraint_kind) const {
  auto string_pattern = as_string_literal(pattern_);
  if (!string_pattern) {
    return MethodHashedSet::top();
  }
  switch (constraint_kind) {
    case MaySatisfyMethodConstraintKind::Parent:
      return method_mappings.class_to_methods().get(
          *string_pattern, MethodHashedSet::bottom());
    case MaySatisfyMethodConstraintKind::Extends:
      return method_mappings.class_to_override_methods().get(
          *string_pattern, MethodHashedSet::bottom());
    default:
      mt_unreachable();
  }
}

bool TypePatternConstraint::satisfy(const DexType* type) const {
  return re2::RE2::FullMatch(type->str(), pattern_);
}

bool TypePatternConstraint::operator==(const TypeConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const TypePatternConstraint*>(&other)) {
    return other_constraint->pattern_.pattern() == pattern_.pattern();
  } else {
    return false;
  }
}

TypeNameConstraint::TypeNameConstraint(std::string name)
    : name_(std::move(name)) {}

MethodHashedSet TypeNameConstraint::may_satisfy(
    const MethodMappings& method_mappings,
    MaySatisfyMethodConstraintKind constraint_kind) const {
  switch (constraint_kind) {
    case MaySatisfyMethodConstraintKind::Parent:
      return method_mappings.class_to_methods().get(
          name_, MethodHashedSet::bottom());
    case MaySatisfyMethodConstraintKind::Extends:
      return method_mappings.class_to_override_methods().get(
          name_, MethodHashedSet::bottom());
    default:
      mt_unreachable();
  }
}

bool TypeNameConstraint::satisfy(const DexType* type) const {
  return type->str() == name_;
}

bool TypeNameConstraint::operator==(const TypeConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const TypeNameConstraint*>(&other)) {
    return other_constraint->name_ == name_;
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

MethodHashedSet ExtendsConstraint::may_satisfy(
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

IsEnumTypeConstraint::IsEnumTypeConstraint(bool expected)
    : expected_(expected) {}

bool IsEnumTypeConstraint::satisfy(const DexType* type) const {
  DexClass* klass = type_class(type);
  bool is_enum_type = klass != nullptr && is_enum(klass);
  return expected_ == is_enum_type;
}

bool IsEnumTypeConstraint::operator==(const TypeConstraint& other) const {
  if (auto* other_constraint =
          dynamic_cast<const IsEnumTypeConstraint*>(&other)) {
    return other_constraint->expected_ == expected_;
  } else {
    return false;
  }
}

AllOfTypeConstraint::AllOfTypeConstraint(
    std::vector<std::unique_ptr<TypeConstraint>> constraints)
    : inner_constraints_(std::move(constraints)) {}

MethodHashedSet AllOfTypeConstraint::may_satisfy(
    const MethodMappings& method_mappings,
    MaySatisfyMethodConstraintKind constraint_kind) const {
  MethodHashedSet intersection_set = MethodHashedSet::top();
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

MethodHashedSet AnyOfTypeConstraint::may_satisfy(
    const MethodMappings& method_mappings,
    MaySatisfyMethodConstraintKind constraint_kind) const {
  if (inner_constraints_.empty()) {
    return MethodHashedSet::top();
  }
  auto union_set = MethodHashedSet::bottom();
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

MethodHashedSet NotTypeConstraint::may_satisfy(
    const MethodMappings& method_mappings,
    MaySatisfyMethodConstraintKind constraint_kind) const {
  MethodHashedSet child_methods =
      constraint_->may_satisfy(method_mappings, constraint_kind);
  if (child_methods.is_top() || child_methods.is_bottom()) {
    return MethodHashedSet::top();
  }
  MethodHashedSet all_methods = method_mappings.all_methods();
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

std::unique_ptr<TypeConstraint> TypeConstraint::from_json(
    const Json::Value& constraint) {
  JsonValidation::validate_object(constraint);

  std::string constraint_name =
      JsonValidation::string(constraint, "constraint");
  if (constraint_name == "name") {
    return std::make_unique<TypePatternConstraint>(
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
      constraint_name == "is_class" || constraint_name == "is_interface" ||
      constraint_name == "is_enum") {
    bool expected = constraint.isMember("value")
        ? JsonValidation::boolean(constraint, /* field */ "value")
        : true;
    if (constraint_name == "is_class") {
      return std::make_unique<IsClassTypeConstraint>(expected);
    } else if (constraint_name == "is_interface") {
      return std::make_unique<IsInterfaceTypeConstraint>(expected);
    } else {
      return std::make_unique<IsEnumTypeConstraint>(expected);
    }
  } else {
    throw JsonValidationError(
        constraint,
        /* field */ "constraint",
        /* expected */ "valid constraint type");
    return nullptr;
  }
}

} // namespace marianatrench

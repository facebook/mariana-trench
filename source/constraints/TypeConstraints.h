/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <re2/re2.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/constraints/IntegerConstraint.h>
#include <mariana-trench/model-generator/ModelGenerator.h>

namespace marianatrench {

enum class MaySatisfyMethodConstraintKind {
  Parent,
  Extends,
};

class TypeConstraint {
 public:
  TypeConstraint() = default;
  virtual ~TypeConstraint() = default;
  TypeConstraint(const TypeConstraint& other) = delete;
  TypeConstraint(TypeConstraint&& other) = delete;
  TypeConstraint& operator=(const TypeConstraint& other) = delete;
  TypeConstraint& operator=(TypeConstraint&& other) = delete;

  static std::unique_ptr<TypeConstraint> from_json(
      const Json::Value& constraint);
  virtual MethodHashedSet may_satisfy(
      const MethodMappings& method_mappings,
      MaySatisfyMethodConstraintKind constraint_kind) const;
  virtual bool satisfy(const DexType* type) const = 0;
  virtual bool operator==(const TypeConstraint& other) const = 0;
};

class TypeNameConstraint final : public TypeConstraint {
 public:
  explicit TypeNameConstraint(const std::string& regex_string);
  MethodHashedSet may_satisfy(
      const MethodMappings& method_mappings,
      MaySatisfyMethodConstraintKind constraint_kind) const override;
  bool satisfy(const DexType* type) const override;
  bool operator==(const TypeConstraint& other) const override;

 private:
  re2::RE2 pattern_;
};

class HasAnnotationTypeConstraint final : public TypeConstraint {
 public:
  explicit HasAnnotationTypeConstraint(
      std::string type,
      std::optional<std::string> annotation);
  bool satisfy(const DexType* type) const override;
  bool operator==(const TypeConstraint& other) const override;

 private:
  std::string type_;
  std::optional<re2::RE2> annotation_;
};

class ExtendsConstraint final : public TypeConstraint {
 public:
  explicit ExtendsConstraint(
      std::unique_ptr<TypeConstraint> inner_constraint,
      bool include_self = true);
  MethodHashedSet may_satisfy(
      const MethodMappings& method_mappings,
      MaySatisfyMethodConstraintKind constraint_kind) const override;
  /* Check if a superclass of the given type, or an interface that the given
   * type implements satisfies the given type constraint */
  bool satisfy(const DexType* type) const override;
  bool operator==(const TypeConstraint& other) const override;

 private:
  std::unique_ptr<TypeConstraint> inner_constraint_;
  bool include_self_;
};

class SuperConstraint final : public TypeConstraint {
 public:
  explicit SuperConstraint(std::unique_ptr<TypeConstraint> inner_constraint);
  /* Check if the direct superclass of the given type satisfies the given type
   * constraint. */
  bool satisfy(const DexType* type) const override;
  bool operator==(const TypeConstraint& other) const override;

 private:
  std::unique_ptr<TypeConstraint> inner_constraint_;
};

class IsClassTypeConstraint final : public TypeConstraint {
 public:
  explicit IsClassTypeConstraint(bool expected = true);
  bool satisfy(const DexType* type) const override;
  bool operator==(const TypeConstraint& other) const override;

 private:
  bool expected_;
};

class IsInterfaceTypeConstraint final : public TypeConstraint {
 public:
  explicit IsInterfaceTypeConstraint(bool expected = true);
  bool satisfy(const DexType* type) const override;
  bool operator==(const TypeConstraint& other) const override;

 private:
  bool expected_;
};

class IsEnumTypeConstraint final : public TypeConstraint {
 public:
  explicit IsEnumTypeConstraint(bool expected = true);
  bool satisfy(const DexType* type) const override;
  bool operator==(const TypeConstraint& other) const override;

 private:
  bool expected_;
};

class AllOfTypeConstraint final : public TypeConstraint {
 public:
  explicit AllOfTypeConstraint(
      std::vector<std::unique_ptr<TypeConstraint>> constraints);
  MethodHashedSet may_satisfy(
      const MethodMappings& method_mappings,
      MaySatisfyMethodConstraintKind constraint_kind) const override;
  bool satisfy(const DexType* type) const override;
  bool operator==(const TypeConstraint& other) const override;

 private:
  std::vector<std::unique_ptr<TypeConstraint>> inner_constraints_;
};

class AnyOfTypeConstraint final : public TypeConstraint {
 public:
  explicit AnyOfTypeConstraint(
      std::vector<std::unique_ptr<TypeConstraint>> constraints);
  MethodHashedSet may_satisfy(
      const MethodMappings& method_mappings,
      MaySatisfyMethodConstraintKind constraint_kind) const override;
  bool satisfy(const DexType* type) const override;
  bool operator==(const TypeConstraint& other) const override;

 private:
  std::vector<std::unique_ptr<TypeConstraint>> inner_constraints_;
};

class NotTypeConstraint final : public TypeConstraint {
 public:
  explicit NotTypeConstraint(std::unique_ptr<TypeConstraint> constraint);
  MethodHashedSet may_satisfy(
      const MethodMappings& method_mappings,
      MaySatisfyMethodConstraintKind constraint_kind) const override;
  bool satisfy(const DexType* type) const override;
  bool operator==(const TypeConstraint& other) const override;

 private:
  std::unique_ptr<TypeConstraint> constraint_;
};

} // namespace marianatrench

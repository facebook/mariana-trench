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
#include <mariana-trench/constraints/IntegerConstraint.h>
#include <mariana-trench/constraints/ParameterConstraints.h>
#include <mariana-trench/constraints/TypeConstraints.h>
#include <mariana-trench/model-generator/ModelGenerator.h>

namespace marianatrench {

bool has_annotation(
    const DexAnnotationSet* annotations_set,
    const std::string& expected_type,
    const std::optional<re2::RE2>& expected_annotation);

class MethodConstraint {
 public:
  MethodConstraint() = default;
  virtual ~MethodConstraint() = default;
  MethodConstraint(const MethodConstraint& other) = delete;
  MethodConstraint(MethodConstraint&& other) = delete;
  MethodConstraint& operator=(const MethodConstraint& other) = delete;
  MethodConstraint& operator=(MethodConstraint&& other) = delete;

  static std::unique_ptr<MethodConstraint> from_json(
      const Json::Value& constraint,
      Context& context);
  virtual bool has_children() const;
  virtual std::vector<const MethodConstraint*> children() const;
  virtual MethodHashedSet may_satisfy(
      const MethodMappings& method_mappings) const;
  virtual bool satisfy(const Method* method) const = 0;
  virtual bool operator==(const MethodConstraint& other) const = 0;
};

class MethodPatternConstraint final : public MethodConstraint {
 public:
  explicit MethodPatternConstraint(const std::string& regex_string);
  MethodHashedSet may_satisfy(
      const MethodMappings& method_mappings) const override;
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;

 private:
  re2::RE2 pattern_;
};

class MethodNameConstraint final : public MethodConstraint {
 public:
  explicit MethodNameConstraint(std::string name);
  MethodHashedSet may_satisfy(
      const MethodMappings& method_mappings) const override;
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;

 private:
  std::string name_;
};

class ParentConstraint final : public MethodConstraint {
 public:
  explicit ParentConstraint(std::unique_ptr<TypeConstraint> inner_constraint);
  MethodHashedSet may_satisfy(
      const MethodMappings& method_mappings) const override;
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;

 private:
  std::unique_ptr<TypeConstraint> inner_constraint_;
};

class AllOfMethodConstraint final : public MethodConstraint {
 public:
  explicit AllOfMethodConstraint(
      std::vector<std::unique_ptr<MethodConstraint>> constraints);
  bool has_children() const override;
  std::vector<const MethodConstraint*> children() const override;
  MethodHashedSet may_satisfy(
      const MethodMappings& method_mappings) const override;
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;

 private:
  std::vector<std::unique_ptr<MethodConstraint>> constraints_;
};

class AnyOfMethodConstraint final : public MethodConstraint {
 public:
  explicit AnyOfMethodConstraint(
      std::vector<std::unique_ptr<MethodConstraint>> constraints);
  bool has_children() const override;
  std::vector<const MethodConstraint*> children() const override;
  MethodHashedSet may_satisfy(
      const MethodMappings& method_mappings) const override;
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;

 private:
  std::vector<std::unique_ptr<MethodConstraint>> constraints_;
};

class NotMethodConstraint final : public MethodConstraint {
 public:
  explicit NotMethodConstraint(std::unique_ptr<MethodConstraint> constraint);
  bool has_children() const override;
  std::vector<const MethodConstraint*> children() const override;
  MethodHashedSet may_satisfy(
      const MethodMappings& method_mappings) const override;
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;

 private:
  std::unique_ptr<MethodConstraint> constraint_;
};

class NumberParametersConstraint final : public MethodConstraint {
 public:
  explicit NumberParametersConstraint(IntegerConstraint constraint);
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;

 private:
  IntegerConstraint constraint_;
};

class NumberOverridesConstraint final : public MethodConstraint {
 public:
  explicit NumberOverridesConstraint(
      IntegerConstraint constraint,
      Context& context);
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;

 private:
  IntegerConstraint constraint_;
  Context& context_;
};

class IsStaticConstraint final : public MethodConstraint {
 public:
  explicit IsStaticConstraint(bool expected);
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;

 private:
  bool expected_;
};

class IsConstructorConstraint final : public MethodConstraint {
 public:
  explicit IsConstructorConstraint(bool expected);
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;

 private:
  bool expected_;
};

class IsNativeConstraint final : public MethodConstraint {
 public:
  explicit IsNativeConstraint(bool expected);
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;

 private:
  bool expected_;
};

class HasCodeConstraint final : public MethodConstraint {
 public:
  explicit HasCodeConstraint(bool expected);
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;

 private:
  bool expected_;
};

class HasAnnotationMethodConstraint final : public MethodConstraint {
 public:
  explicit HasAnnotationMethodConstraint(
      std::string type,
      std::optional<std::string> annotation);
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;
  MethodHashedSet may_satisfy(
      const MethodMappings& method_mappings) const override;

 private:
  std::string type_;
  std::optional<re2::RE2> annotation_;
};

class NthParameterConstraint final : public MethodConstraint {
 public:
  NthParameterConstraint(
      ParameterPosition index,
      std::unique_ptr<ParameterConstraint> inner_constraint);
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;

 private:
  ParameterPosition index_;
  std::unique_ptr<ParameterConstraint> inner_constraint_;
};

class SignaturePatternConstraint final : public MethodConstraint {
 public:
  explicit SignaturePatternConstraint(const std::string& regex_string);
  MethodHashedSet may_satisfy(
      const MethodMappings& method_mappings) const override;
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;

 private:
  re2::RE2 pattern_;
};

class ReturnConstraint final : public MethodConstraint {
 public:
  explicit ReturnConstraint(std::unique_ptr<TypeConstraint> inner_constraint);
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;

 private:
  std::unique_ptr<TypeConstraint> inner_constraint_;
};

class MethodHasStringConstraint final : public MethodConstraint {
 public:
  explicit MethodHasStringConstraint(const std::string& regex_string);
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;

 private:
  re2::RE2 pattern_;
};

class VisibilityMethodConstraint final : public MethodConstraint {
 public:
  explicit VisibilityMethodConstraint(DexAccessFlags visibility);
  bool satisfy(const Method* method) const override;
  bool operator==(const MethodConstraint& other) const override;

 private:
  DexAccessFlags visibility_;
};

} // namespace marianatrench

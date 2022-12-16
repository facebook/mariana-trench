/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/constraints/TypeConstraints.h>

namespace marianatrench {

class ParameterConstraint {
 public:
  ParameterConstraint() = default;
  virtual ~ParameterConstraint() = default;
  ParameterConstraint(const ParameterConstraint& other) = delete;
  ParameterConstraint(ParameterConstraint&& other) = delete;
  ParameterConstraint& operator=(const ParameterConstraint& other) = delete;
  ParameterConstraint& operator=(ParameterConstraint&& other) = delete;

  static std::unique_ptr<ParameterConstraint> from_json(
      const Json::Value& constraint);
  virtual bool satisfy(
      const DexAnnotationSet* MT_NULLABLE annotations_set,
      const DexType* type) const = 0;

  virtual bool operator==(const ParameterConstraint& other) const = 0;
};

class AllOfParameterConstraint final : public ParameterConstraint {
 public:
  explicit AllOfParameterConstraint(
      std::vector<std::unique_ptr<ParameterConstraint>> constraints);
  bool satisfy(
      const DexAnnotationSet* MT_NULLABLE annotations_set,
      const DexType* type) const override;
  bool operator==(const ParameterConstraint& other) const override;

 private:
  std::vector<std::unique_ptr<ParameterConstraint>> constraints_;
};

class AnyOfParameterConstraint final : public ParameterConstraint {
 public:
  explicit AnyOfParameterConstraint(
      std::vector<std::unique_ptr<ParameterConstraint>> constraints);
  bool satisfy(
      const DexAnnotationSet* MT_NULLABLE annotations_set,
      const DexType* type) const override;
  bool operator==(const ParameterConstraint& other) const override;

 private:
  std::vector<std::unique_ptr<ParameterConstraint>> constraints_;
};

class NotParameterConstraint final : public ParameterConstraint {
 public:
  explicit NotParameterConstraint(
      std::unique_ptr<ParameterConstraint> constraint);
  bool satisfy(
      const DexAnnotationSet* MT_NULLABLE annotations_set,
      const DexType* type) const override;
  bool operator==(const ParameterConstraint& other) const override;

 private:
  std::unique_ptr<ParameterConstraint> constraint_;
};

class HasAnnotationParameterConstraint final : public ParameterConstraint {
 public:
  explicit HasAnnotationParameterConstraint(
      std::string type,
      std::optional<std::string> annotation);
  bool satisfy(
      const DexAnnotationSet* MT_NULLABLE annotations_set,
      const DexType* /* unused */) const override;
  bool operator==(const ParameterConstraint& other) const override;

 private:
  std::string type_;
  std::optional<re2::RE2> annotation_;
};

class TypeParameterConstraint final : public ParameterConstraint {
 public:
  explicit TypeParameterConstraint(
      std::unique_ptr<TypeConstraint> inner_constraint);
  bool satisfy(
      const DexAnnotationSet* MT_NULLABLE /* unused */,
      const DexType* type) const override;
  bool operator==(const ParameterConstraint& other) const override;

 private:
  std::unique_ptr<TypeConstraint> inner_constraint_;
};

} // namespace marianatrench

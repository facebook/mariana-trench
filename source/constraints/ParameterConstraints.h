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

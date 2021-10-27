/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <re2/re2.h>

#include <mariana-trench/Field.h>

namespace marianatrench {

class FieldConstraint {
 public:
  FieldConstraint() = default;
  virtual ~FieldConstraint() = default;
  FieldConstraint(const FieldConstraint& other) = delete;
  FieldConstraint(FieldConstraint&& other) = delete;
  FieldConstraint& operator=(const FieldConstraint& other) = delete;
  FieldConstraint& operator=(FieldConstraint&& other) = delete;

  static std::unique_ptr<FieldConstraint> from_json(
      const Json::Value& constraint);
  virtual bool satisfy(const Field* field) const = 0;
  virtual bool operator==(const FieldConstraint& other) const = 0;
};

class FieldNameConstraint final : public FieldConstraint {
 public:
  explicit FieldNameConstraint(const std::string& regex_string);
  bool satisfy(const Field* field) const override;
  bool operator==(const FieldConstraint& other) const override;

 private:
  re2::RE2 pattern_;
};

class HasAnnotationFieldConstraint final : public FieldConstraint {
 public:
  explicit HasAnnotationFieldConstraint(
      const std::string& type,
      const std::optional<std::string>& annotation);
  bool satisfy(const Field* field) const override;
  bool operator==(const FieldConstraint& other) const override;

 private:
  std::string type_;
  std::optional<re2::RE2> annotation_;
};

class AllOfFieldConstraint final : public FieldConstraint {
 public:
  explicit AllOfFieldConstraint(
      std::vector<std::unique_ptr<FieldConstraint>> constraints);
  bool satisfy(const Field* field) const override;
  bool operator==(const FieldConstraint& other) const override;

 private:
  std::vector<std::unique_ptr<FieldConstraint>> constraints_;
};

} // namespace marianatrench

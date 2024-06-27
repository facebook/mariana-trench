/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <DexAnnotation.h>
#include <DexClass.h>

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Model.h>

namespace marianatrench {

/**
 * Used for validating @Expected* annotations in the APK (if any) against the
 * models emitted at the end of the analysis. Each @Expected* annotation type
 * should inherit from this class.
 */
class ModelValidator {
 public:
  ModelValidator() = default;
  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS_VIRTUAL_DESTRUCTOR(ModelValidator)

  static std::vector<std::unique_ptr<ModelValidator>> from_annotation(
      const DexAnnotation* annotation);

  virtual bool validate(const Model& model) const = 0;

  virtual std::string show() const = 0;
};

class ExpectIssue final : public ModelValidator {
 public:
  explicit ExpectIssue(const EncodedAnnotations& annotation_elements);

  explicit ExpectIssue(
      int code,
      std::set<std::string> source_kinds,
      std::set<std::string> sink_kinds)
      : code_(code),
        source_kinds_(std::move(source_kinds)),
        sink_kinds_(std::move(sink_kinds)) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ExpectIssue)

  bool validate(const Model& model) const override;

  std::string show() const override;

 private:
  int code_;

  // NOTE: Ordering is used for subset/includes comparison against issue kinds
  std::set<std::string> source_kinds_;
  std::set<std::string> sink_kinds_;
};

class ExpectNoIssue final : public ModelValidator {
 public:
  explicit ExpectNoIssue(const EncodedAnnotations& annotation_elements);

  explicit ExpectNoIssue(int code) : code_(code) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ExpectNoIssue)

  bool validate(const Model& model) const override;

  std::string show() const override;

 private:
  // This field in @ExpectNoIssue annotation is optional and defaults to -1
  // if unspecified.
  int code_;
};

} // namespace marianatrench

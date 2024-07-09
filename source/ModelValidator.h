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

class ModelValidator;

/**
 * The top-level validator for models in given method.
 */
class ModelValidators final {
 public:
  explicit ModelValidators(
      std::vector<std::unique_ptr<ModelValidator>> validators)
      : validators_(std::move(validators)) {
    mt_assert(!validators_.empty());
  }

  MOVE_CONSTRUCTOR_ONLY(ModelValidators)

  /**
   * Contains all validators for the given method. `std::nullopt` if the method
   * has no validators, e.g. "fake" methods, no validator annotations, etc.
   */
  static std::optional<ModelValidators> from_method(const Method* method);

  bool validate(const Model& model) const;

  std::string show() const;

 private:
  std::vector<std::unique_ptr<ModelValidator>> validators_;
};

/**
 * Used for validating @Expected* annotations in the APK (if any) against the
 * models emitted at the end of the analysis. Each @Expected* annotation type
 * should inherit from this class.
 */
class ModelValidator {
 public:
  ModelValidator() = default;
  MOVE_CONSTRUCTOR_ONLY_VIRTUAL_DESTRUCTOR(ModelValidator)

  virtual bool validate(const Model& model) const = 0;

  virtual std::string show() const = 0;
};

class ExpectIssue final : public ModelValidator {
 public:
  explicit ExpectIssue(
      int code,
      std::set<std::string> source_kinds,
      std::set<std::string> sink_kinds)
      : code_(code),
        source_kinds_(std::move(source_kinds)),
        sink_kinds_(std::move(sink_kinds)) {
    // "Code" must have been specified.
    mt_assert(code != -1);
  }

  MOVE_CONSTRUCTOR_ONLY(ExpectIssue)

  static ExpectIssue from_annotation(
      const EncodedAnnotations& annotation_elements);

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
  explicit ExpectNoIssue(int code) : code_(code) {}

  MOVE_CONSTRUCTOR_ONLY(ExpectNoIssue)

  static ExpectNoIssue from_annotation(
      const EncodedAnnotations& annotation_elements);

  bool validate(const Model& model) const override;

  std::string show() const override;

 private:
  // This field in @ExpectNoIssue annotation is optional and defaults to -1
  // if unspecified.
  int code_;
};

} // namespace marianatrench

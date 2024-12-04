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

class ModelValidatorResult {
 public:
  explicit ModelValidatorResult(bool valid, std::string annotation)
      : valid_(valid), annotation_(std::move(annotation)) {}

  MOVE_CONSTRUCTOR_ONLY(ModelValidatorResult)

  bool is_valid() const {
    return valid_;
  }

  Json::Value to_json() const;

 private:
  bool valid_;
  std::string annotation_;
};

class ModelValidatorsResult {
 public:
  explicit ModelValidatorsResult(
      const Method* method,
      std::vector<ModelValidatorResult> results)
      : method_(method), results_(std::move(results)) {}

  MOVE_CONSTRUCTOR_ONLY(ModelValidatorsResult)

  Json::Value to_json() const;

 private:
  const Method* method_;
  std::vector<ModelValidatorResult> results_;
};

/**
 * The top-level validator for models in given method.
 */
class ModelValidators final {
 public:
  explicit ModelValidators(
      const Method* method,
      std::vector<std::unique_ptr<ModelValidator>> validators)
      : method_(method), validators_(std::move(validators)) {
    mt_assert(!validators_.empty());
  }

  MOVE_CONSTRUCTOR_ONLY(ModelValidators)

  /**
   * Contains all validators for the given method. `std::nullopt` if the method
   * has no validators, e.g. "fake" methods, no validator annotations, etc.
   */
  static std::optional<ModelValidators> from_method(const Method* method);

  ModelValidatorsResult validate(const Model& model) const;

  std::string show() const;

 private:
  const Method* method_;
  std::vector<std::unique_ptr<ModelValidator>> validators_;
};

/**
 * Used for validating @Expect* annotations in the APK (if any) against the
 * models emitted at the end of the analysis. Each @Expect* annotation type
 * should inherit from this class.
 */
class ModelValidator {
 public:
  explicit ModelValidator(bool is_false_classification)
      : is_false_classification_(is_false_classification) {}

  MOVE_CONSTRUCTOR_ONLY_VIRTUAL_DESTRUCTOR(ModelValidator)

  virtual ModelValidatorResult validate(const Model& model) const = 0;

  virtual std::string show() const = 0;

 protected:
  // i.e. is_false_[positive|negative]. Classification type depends on whether
  // the validator is looking for the presence or absence of a model. If
  // looking for presence (e.g. ExpectIssue), flag represents is_false_positive,
  // and vice versa.
  bool is_false_classification_;
};

/**
 * Represents the properties of an issue that need to be validated by a
 * ModelValidator. Whether validation is based on the absence/presence of these
 * properties depends on the ModelValidator's type.
 */
class IssueProperties final {
 public:
  explicit IssueProperties(
      int code,
      std::set<std::string> source_kinds,
      std::set<std::string> sink_kinds,
      std::set<std::string> source_origins,
      std::set<std::string> sink_origins)
      : code_(code),
        source_kinds_(std::move(source_kinds)),
        sink_kinds_(std::move(sink_kinds)),
        source_origins_(std::move(source_origins)),
        sink_origins_(std::move(sink_origins)) {
    // "Code" must have been specified.
    mt_assert(code != -1);
  }

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(IssueProperties)

  /**
   * Validates the presence of an issue in the given model.
   */
  bool validate_presence(const Model& model) const;

  std::string show() const;

 private:
  int code_;

  // NOTE: Ordering is used for subset/includes comparison against issue kinds
  std::set<std::string> source_kinds_;
  std::set<std::string> sink_kinds_;
  std::set<std::string> source_origins_;
  std::set<std::string> sink_origins_;
};

class ExpectIssue final : public ModelValidator {
 public:
  explicit ExpectIssue(bool is_false_positive, IssueProperties issue_properties)
      : ModelValidator(is_false_positive),
        issue_properties_(std::move(issue_properties)) {}

  MOVE_CONSTRUCTOR_ONLY(ExpectIssue)

  static ExpectIssue from_annotation(
      const EncodedAnnotations& annotation_elements);

  ModelValidatorResult validate(const Model& model) const override;

  std::string show() const override;

 private:
  IssueProperties issue_properties_;
};

class ExpectNoIssue final : public ModelValidator {
 public:
  explicit ExpectNoIssue(
      bool is_false_negative,
      IssueProperties issue_properties)
      : ModelValidator(is_false_negative),
        issue_properties_(std::move(issue_properties)) {}

  MOVE_CONSTRUCTOR_ONLY(ExpectNoIssue)

  static ExpectNoIssue from_annotation(
      const EncodedAnnotations& annotation_elements);

  ModelValidatorResult validate(const Model& model) const override;

  std::string show() const override;

 private:
  IssueProperties issue_properties_;
};

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string.hpp>
#include <fmt/format.h>
#include <algorithm>

#include <Show.h>

#include <mariana-trench/ModelValidator.h>

namespace marianatrench {

Json::Value ModelValidatorResult::to_json() const {
  auto result = Json::Value(Json::objectValue);
  result["valid"] = valid_;
  result["annotation"] = annotation_;
  return result;
}

Json::Value ModelValidatorsResult::to_json() const {
  auto results = Json::Value(Json::objectValue);
  results["method"] = method_->show();

  auto validator_results = Json::Value(Json::arrayValue);
  for (const auto& result : results_) {
    validator_results.append(result.to_json());
  }
  results["validators"] = validator_results;

  return results;
}

namespace {

enum ModelValidationType {
  EXPECT_ISSUE,
  EXPECT_NO_ISSUE,
};

struct AnnotationProperties {
  ModelValidationType validation_type;
  bool repeatable;
};

std::optional<AnnotationProperties> get_annotation_properties(
    const DexAnnotation* annotation) {
  const auto* annotation_type = annotation->type();
  if (!annotation_type) {
    return std::nullopt;
  }

  auto annotation_type_string = annotation_type->str();
  if (annotation_type_string ==
      "Lcom/facebook/marianabench/validation/ExpectIssue;") {
    return AnnotationProperties{
        .validation_type = ModelValidationType::EXPECT_ISSUE,
        .repeatable = false};
  } else if (
      annotation_type_string ==
      "Lcom/facebook/marianabench/validation/ExpectIssues;") {
    // For repeatable/multiple @ExpectIssue annotation
    return AnnotationProperties{
        .validation_type = ModelValidationType::EXPECT_ISSUE,
        .repeatable = true};
  } else if (
      annotation_type_string ==
      "Lcom/facebook/marianabench/validation/ExpectNoIssue;") {
    return AnnotationProperties{
        .validation_type = ModelValidationType::EXPECT_NO_ISSUE,
        .repeatable = false};
  } else if (
      annotation_type_string ==
      "Lcom/facebook/marianabench/validation/ExpectNoIssues;") {
    // For repeatable/multiple @ExpectNoIssue annotation
    return AnnotationProperties{
        .validation_type = ModelValidationType::EXPECT_NO_ISSUE,
        .repeatable = true};
  }

  return std::nullopt;
}

std::unique_ptr<ModelValidator> make_validator(
    ModelValidationType validator_type,
    const EncodedAnnotations& annotation_elements) {
  switch (validator_type) {
    case ModelValidationType::EXPECT_ISSUE:
      return std::make_unique<ExpectIssue>(
          ExpectIssue::from_annotation(annotation_elements));
    case ModelValidationType::EXPECT_NO_ISSUE:
      return std::make_unique<ExpectNoIssue>(
          ExpectNoIssue::from_annotation(annotation_elements));
      break;
  }
}

std::vector<std::unique_ptr<ModelValidator>> validators_from_annotation(
    const DexAnnotation* annotation) {
  auto annotation_properties = get_annotation_properties(annotation);
  if (!annotation_properties) {
    return {};
  }

  std::vector<std::unique_ptr<ModelValidator>> validators;
  ModelValidationType validation_type = annotation_properties->validation_type;

  if (!annotation_properties->repeatable) {
    // Non-repeating annotations are encoded in the outer-most annotation.
    validators.emplace_back(
        make_validator(validation_type, annotation->anno_elems()));
  } else {
    // Repeatable annotation. The annotation of interest is nested within an
    // encoded array. The outer annotation has only 1 element which is the
    // array itself.
    mt_assert(annotation->anno_elems().size() == 1);
    for (const auto& element : annotation->anno_elems()) {
      // Arrays represent repeated annotations of the same kind.
      const auto* repeatable_annotation =
          dynamic_cast<const DexEncodedValueArray*>(
              element.encoded_value.get());
      mt_assert(repeatable_annotation != nullptr);

      for (const auto& repeated_annotation :
           *repeatable_annotation->evalues()) {
        const auto* inner_annotation =
            dynamic_cast<const DexEncodedValueAnnotation*>(
                repeated_annotation.get());
        // Within each repeatable annotation should be the nested annotation.
        mt_assert(inner_annotation != nullptr);
        validators.emplace_back(
            make_validator(validation_type, inner_annotation->annotations()));
      }
    }
  }

  return validators;
}

} // namespace

std::optional<ModelValidators> ModelValidators::from_method(
    const Method* method) {
  const auto* dex_method = method->dex_method();
  if (dex_method == nullptr) {
    return std::nullopt;
  }

  const auto* annotations_set = dex_method->get_anno_set();
  if (annotations_set == nullptr) {
    return std::nullopt;
  }

  std::vector<std::unique_ptr<ModelValidator>> validators;
  for (const auto& annotation : annotations_set->get_annotations()) {
    auto validator = validators_from_annotation(annotation.get());
    validators.insert(
        validators.end(),
        std::make_move_iterator(validator.begin()),
        std::make_move_iterator(validator.end()));
  }

  if (validators.empty()) {
    return std::nullopt;
  }

  return ModelValidators(method, std::move(validators));
}

ModelValidatorsResult ModelValidators::validate(const Model& model) const {
  std::vector<ModelValidatorResult> results;
  for (const auto& validator : validators_) {
    results.emplace_back(validator->validate(model));
  }

  return ModelValidatorsResult(method_, std::move(results));
}

std::string ModelValidators::show() const {
  std::vector<std::string> validator_strings(validators_.size());
  std::transform(
      validators_.cbegin(),
      validators_.cend(),
      validator_strings.begin(),
      [](const auto& validator) { return validator->show(); });
  return boost::algorithm::join(validator_strings, ",");
}

namespace {

void gather_strings(
    const DexAnnotationElement& annotation_element,
    std::set<std::string>& result) {
  mt_assert(annotation_element.encoded_value->evtype() == DEVT_ARRAY);
  std::vector<const DexString*> dex_strings;
  annotation_element.encoded_value->gather_strings(dex_strings);
  for (const auto* dex_string : dex_strings) {
    result.insert(dex_string->str_copy());
  }
}

} // namespace

ExpectIssue ExpectIssue::from_annotation(
    const EncodedAnnotations& annotation_elements) {
  int code = -1;
  std::set<std::string> source_kinds;
  std::set<std::string> sink_kinds;
  std::set<std::string> source_origins;
  std::set<std::string> sink_origins;

  for (const auto& annotation_element : annotation_elements) {
    const auto* annotation_key = annotation_element.string;
    if (annotation_key->str() == "code") {
      mt_assert(annotation_element.encoded_value->is_evtype_primitive());
      code = annotation_element.encoded_value->value();
    } else if (annotation_key->str() == "sourceKinds") {
      gather_strings(annotation_element, source_kinds);
    } else if (annotation_key->str() == "sinkKinds") {
      gather_strings(annotation_element, sink_kinds);
    } else if (annotation_key->str() == "sourceOrigins") {
      gather_strings(annotation_element, source_origins);
    } else if (annotation_key->str() == "sinkOrigins") {
      gather_strings(annotation_element, sink_origins);
    } else if (
        annotation_key->str() == "isFalsePositive" ||
        annotation_key->str() == "isFalseNegative") {
      // No-op. This is a flag to document whether the expected issue is a false
      // positive (or false negative for ExpectNoIssue). It has no bearing on
      // validation.
    } else {
      // Do not fail in case new fields have been added to annotation, in which
      // case, the error is expected to resolve on the next release
      ERROR(
          1,
          "Unexpected annotation key: {} in @ExpectIssue",
          ::show(annotation_key));
    }
  }

  return ExpectIssue(
      code,
      std::move(source_kinds),
      std::move(sink_kinds),
      std::move(source_origins),
      std::move(sink_origins));
}

namespace {

bool includes_issue_kinds(
    const std::unordered_set<const Kind*>& issue_kinds,
    const std::set<std::string>& validator_kinds) {
  if (validator_kinds.empty()) {
    return true;
  }

  std::set<std::string> issue_kinds_set;
  for (const auto* kind : issue_kinds) {
    issue_kinds_set.insert(kind->to_trace_string());
  }

  return std::includes(
      issue_kinds_set.cbegin(),
      issue_kinds_set.cend(),
      validator_kinds.cbegin(),
      validator_kinds.cend());
}

bool includes_origins(
    const Taint& taint,
    const std::set<std::string>& validator_origins) {
  std::set<std::string> taint_origins;
  taint.visit_frames([&taint_origins](
                         const CallInfo& /* call_info */, const Frame& frame) {
    for (const auto* origin : frame.origins()) {
      if (auto model_validator_string = origin->to_model_validator_string()) {
        taint_origins.insert(*model_validator_string);
      }
    }
  });
  return std::includes(
      taint_origins.cbegin(),
      taint_origins.cend(),
      validator_origins.cbegin(),
      validator_origins.cend());
}

} // namespace

ModelValidatorResult ExpectIssue::validate(const Model& model) const {
  const auto& issues = model.issues();
  bool valid =
      std::any_of(issues.begin(), issues.end(), [this](const Issue& issue) {
        // Issue (hence rule) should not be bottom() at this point.
        const auto* rule = issue.rule();
        mt_assert(rule != nullptr);
        return rule->code() == code_ &&
            includes_issue_kinds(issue.sources().kinds(), source_kinds_) &&
            includes_issue_kinds(issue.sinks().kinds(), sink_kinds_) &&
            includes_origins(issue.sources(), source_origins_) &&
            includes_origins(issue.sinks(), sink_origins_);
      });
  return ModelValidatorResult(valid, /* annotation */ show());
}

std::string ExpectIssue::show() const {
  return fmt::format("ExpectIssue({})", show_parameters());
}

std::string ExpectIssue::show_parameters() const {
  std::stringstream parameters_string;
  parameters_string << "code=" << code_;
  if (!source_kinds_.empty()) {
    parameters_string << ", sourceKinds={"
                      << boost::algorithm::join(source_kinds_, ",") << "}";
  }
  if (!sink_kinds_.empty()) {
    parameters_string << ", sinkKinds={"
                      << boost::algorithm::join(sink_kinds_, ",") << "}";
  }
  if (!source_origins_.empty()) {
    parameters_string << ", sourceOrigins={"
                      << boost::algorithm::join(source_origins_, ",") << "}";
  }
  if (!sink_origins_.empty()) {
    parameters_string << ", sinkOrigins={"
                      << boost::algorithm::join(sink_origins_, ",") << "}";
  }
  return parameters_string.str();
}

ExpectNoIssue ExpectNoIssue::from_annotation(
    const EncodedAnnotations& annotation_elements) {
  auto expect_issue = ExpectIssue::from_annotation(annotation_elements);
  return ExpectNoIssue(std::move(expect_issue));
}

ModelValidatorResult ExpectNoIssue::validate(const Model& model) const {
  return ModelValidatorResult(
      !expect_issue_.validate(model).is_valid(), /* annotation */ show());
}

std::string ExpectNoIssue::show() const {
  return fmt::format("ExpectNoIssue({})", expect_issue_.show_parameters());
}

} // namespace marianatrench

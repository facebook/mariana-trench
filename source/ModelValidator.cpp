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

namespace {

enum ModelValidationType {
  EXPECT_ISSUE,
  EXPECT_ISSUES,
  EXPECT_NO_ISSUE,
  EXPECT_NO_ISSUES,
};

std::optional<ModelValidationType> validation_type(
    const DexAnnotation* annotation) {
  const auto* annotation_type = annotation->type();
  if (!annotation_type) {
    return std::nullopt;
  }

  auto annotation_type_string = annotation_type->str();
  if (annotation_type_string ==
      "Lcom/facebook/marianabench/validation/ExpectIssue;") {
    return ModelValidationType::EXPECT_ISSUE;
  } else if (
      annotation_type_string ==
      "Lcom/facebook/marianabench/validation/ExpectIssues;") {
    // For repeatable/multiple @ExpectIssue annotation
    return ModelValidationType::EXPECT_ISSUES;
  } else if (
      annotation_type_string ==
      "Lcom/facebook/marianabench/validation/ExpectNoIssue;") {
    return ModelValidationType::EXPECT_NO_ISSUE;
  } else if (
      annotation_type_string ==
      "Lcom/facebook/marianabench/validation/ExpectNoIssues;") {
    // For repeatable/multiple @ExpectNoIssue annotation
    return ModelValidationType::EXPECT_NO_ISSUES;
  }

  return std::nullopt;
}

std::unique_ptr<ModelValidator> make_validator(
    ModelValidationType validator_type,
    const EncodedAnnotations& annotation_elements) {
  switch (validator_type) {
    case ModelValidationType::EXPECT_ISSUE:
    case ModelValidationType::EXPECT_ISSUES:
      return std::make_unique<ExpectIssue>(
          ExpectIssue::from_annotation(annotation_elements));
    case ModelValidationType::EXPECT_NO_ISSUE:
    case ModelValidationType::EXPECT_NO_ISSUES:
      return std::make_unique<ExpectNoIssue>(
          ExpectNoIssue::from_annotation(annotation_elements));
      break;
  }
}

} // namespace

std::vector<std::unique_ptr<ModelValidator>> ModelValidator::from_annotation(
    const DexAnnotation* annotation) {
  auto annotation_type = validation_type(annotation);
  if (!annotation_type) {
    return std::vector<std::unique_ptr<ModelValidator>>{};
  }

  std::vector<std::unique_ptr<ModelValidator>> result;
  switch (*annotation_type) {
    case ModelValidationType::EXPECT_ISSUE:
    case ModelValidationType::EXPECT_NO_ISSUE:
      // Non-repeating annotations are encoded in the outer-most annotation.
      result.emplace_back(
          make_validator(*annotation_type, annotation->anno_elems()));
      break;
    case ModelValidationType::EXPECT_ISSUES:
    case ModelValidationType::EXPECT_NO_ISSUES:
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
          result.emplace_back(make_validator(
              *annotation_type, inner_annotation->annotations()));
        }
      }
      break;
  }

  return result;
}

ExpectIssue ExpectIssue::from_annotation(
    const EncodedAnnotations& annotation_elements) {
  int code = -1;
  std::set<std::string> source_kinds;
  std::set<std::string> sink_kinds;

  for (const auto& annotation_element : annotation_elements) {
    const auto* annotation_key = annotation_element.string;
    if (annotation_key->str() == "code") {
      mt_assert(annotation_element.encoded_value->is_evtype_primitive());
      code = annotation_element.encoded_value->value();
    } else if (annotation_key->str() == "sourceKinds") {
      mt_assert(annotation_element.encoded_value->evtype() == DEVT_ARRAY);
      std::vector<const DexString*> source_kinds_dexstring;
      annotation_element.encoded_value->gather_strings(source_kinds_dexstring);
      for (const auto* dex_string : source_kinds_dexstring) {
        source_kinds.insert(dex_string->str_copy());
      }
    } else if (annotation_key->str() == "sinkKinds") {
      mt_assert(annotation_element.encoded_value->evtype() == DEVT_ARRAY);
      std::vector<const DexString*> sink_kinds_dexstring;
      annotation_element.encoded_value->gather_strings(sink_kinds_dexstring);
      for (const auto* dex_string : sink_kinds_dexstring) {
        sink_kinds.insert(dex_string->str_copy());
      }
    } else {
      throw std::runtime_error(fmt::format(
          "Unexpected annotation key: {} in @ExpectIssue",
          ::show(annotation_key)));
    }
  }

  return ExpectIssue(code, std::move(source_kinds), std::move(sink_kinds));
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

} // namespace

bool ExpectIssue::validate(const Model& model) const {
  const auto& issues = model.issues();
  return std::any_of(issues.begin(), issues.end(), [this](const Issue& issue) {
    // Issue (hence rule) should not be bottom() at this point.
    const auto* rule = issue.rule();
    mt_assert(rule != nullptr);
    return rule->code() == code_ &&
        includes_issue_kinds(issue.sources().kinds(), source_kinds_) &&
        includes_issue_kinds(issue.sinks().kinds(), sink_kinds_);
  });
}

std::string ExpectIssue::show() const {
  return fmt::format(
      "ExpectIssue(code={}, sourceKinds={}, sinkKinds={})",
      code_,
      boost::algorithm::join(source_kinds_, ","),
      boost::algorithm::join(sink_kinds_, ","));
}

ExpectNoIssue ExpectNoIssue::from_annotation(
    const EncodedAnnotations& annotation_elements) {
  int code = -1;
  for (const auto& annotation_element : annotation_elements) {
    const auto* annotation_key = annotation_element.string;
    if (annotation_key->str() == "code") {
      mt_assert(annotation_element.encoded_value->is_evtype_primitive());
      code = annotation_element.encoded_value->value();
    } else {
      throw std::runtime_error(fmt::format(
          "Unexpected annotation key: {} in @ExpectNoIssue",
          ::show(annotation_key)));
    }
  }

  return ExpectNoIssue(code);
}

bool ExpectNoIssue::validate(const Model& model) const {
  if (code_ == -1) {
    // Issue code unspecified. Model must not contain any issues.
    return model.issues().empty();
  }

  const auto& issues = model.issues();
  return std::none_of(issues.begin(), issues.end(), [this](const Issue& issue) {
    // Issue (hence rule) should not be bottom() at this point.
    const auto* rule = issue.rule();
    mt_assert(rule != nullptr);
    return rule->code() == code_;
  });
}

std::string ExpectNoIssue::show() const {
  return fmt::format("ExpectNoIssue(code={})", code_);
}

} // namespace marianatrench

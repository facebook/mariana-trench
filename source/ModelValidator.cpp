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
      return std::make_unique<ExpectIssue>(annotation_elements);
    case ModelValidationType::EXPECT_NO_ISSUE:
    case ModelValidationType::EXPECT_NO_ISSUES:
      return std::make_unique<ExpectNoIssue>(annotation_elements);
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

ExpectIssue::ExpectIssue(const EncodedAnnotations& annotation_elements)
    : code_(-1) {
  for (const auto& annotation_element : annotation_elements) {
    const auto* annotation_key = annotation_element.string;
    if (annotation_key->str() == "code") {
      mt_assert(annotation_element.encoded_value->is_evtype_primitive());
      code_ = annotation_element.encoded_value->value();
    } else if (annotation_key->str() == "sourceKinds") {
      mt_assert(annotation_element.encoded_value->evtype() == DEVT_ARRAY);
      annotation_element.encoded_value->gather_strings(source_kinds_);
    } else if (annotation_key->str() == "sinkKinds") {
      mt_assert(annotation_element.encoded_value->evtype() == DEVT_ARRAY);
      annotation_element.encoded_value->gather_strings(sink_kinds_);
    } else {
      throw std::runtime_error(fmt::format(
          "Unexpected annotation key: {} in @ExpectIssue",
          ::show(annotation_key)));
    }
  }

  // "Code" must have been specified.
  mt_assert(code_ != -1);
}

namespace {

std::string join(
    const std::vector<const DexString*>& dex_strings,
    const std::string& separator) {
  std::vector<std::string> strings(dex_strings.size());
  std::transform(
      dex_strings.cbegin(),
      dex_strings.cend(),
      strings.begin(),
      [](const DexString* dex_string) { return dex_string->str(); });

  return boost::algorithm::join(strings, separator);
}

} // namespace

std::string ExpectIssue::show() const {
  return fmt::format(
      "ExpectIssue(code={}, sourceKinds={}, sinkKinds={})",
      code_,
      join(source_kinds_, ","),
      join(sink_kinds_, ","));
}

ExpectNoIssue::ExpectNoIssue(const EncodedAnnotations& annotation_elements)
    : code_(std::nullopt) {
  for (const auto& annotation_element : annotation_elements) {
    const auto* annotation_key = annotation_element.string;
    if (annotation_key->str() == "code") {
      mt_assert(annotation_element.encoded_value->is_evtype_primitive());
      code_ = annotation_element.encoded_value->value();
    } else {
      throw std::runtime_error(fmt::format(
          "Unexpected annotation key: {} in @ExpectNoIssue",
          ::show(annotation_key)));
    }
  }
}

std::string ExpectNoIssue::show() const {
  return fmt::format("ExpectNoIssue(code={})", code_.has_value() ? *code_ : -1);
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <fmt/format.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/SanitizerSetTransform.h>
#include <mariana-trench/TransformOperations.h>
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {

void SanitizerSetTransform::show(std::ostream& out) const {
  out << to_trace_string();
}

const SanitizerSetTransform* SanitizerSetTransform::from_trace_string(
    const std::string& transform,
    Context& context) {
  if (boost::starts_with(transform, "Sanitize[") &&
      boost::ends_with(transform, "]")) {
    auto kind_str = boost::replace_first_copy(transform, "Sanitize[", "");
    boost::replace_last(kind_str, "]", "");

    const auto kind = SourceSinkKind::from_trace_string(
        kind_str, context, SanitizerKind::Propagations);
    return context.transforms_factory->create_sanitizer_set_transform(
        Set{kind});
  }

  throw JsonValidationError(
      transform,
      /* field */ std::nullopt,
      "Could not be parsed as a valid SanitizeTransform");
}

std::string SanitizerSetTransform::to_trace_string() const {
  std::vector<std::string> sanitized_kinds;
  sanitized_kinds.reserve(kinds_.size());

  for (const auto kind : kinds_) {
    sanitized_kinds.push_back(fmt::format(
        "Sanitize[{}]", kind.to_trace_string(SanitizerKind::Propagations)));
  }

  std::sort(sanitized_kinds.begin(), sanitized_kinds.end());

  return boost::join(sanitized_kinds, ":");
}

const SanitizerSetTransform* SanitizerSetTransform::from_config_json(
    const Json::Value& transform,
    Context& context) {
  const auto kind = SourceSinkKind::from_config_json(
      transform, context, SanitizerKind::Propagations);
  return context.transforms_factory->create_sanitizer_set_transform(Set{kind});
}

} // namespace marianatrench

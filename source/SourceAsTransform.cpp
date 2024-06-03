/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <fmt/format.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/SourceAsTransform.h>
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {

void SourceAsTransform::show(std::ostream& out) const {
  out << to_trace_string();
}

const SourceAsTransform* SourceAsTransform::from_trace_string(
    const std::string& transform,
    Context& context) {
  if (boost::starts_with(transform, "SourceAsTransform[") &&
      boost::ends_with(transform, "]")) {
    auto kind_str =
        boost::replace_first_copy(transform, "SourceAsTransform[", "");
    boost::replace_last(kind_str, "]", "");
    const auto* kind = Kind::from_trace_string(kind_str, context);

    return context.transforms_factory->create_source_as_transform(kind);
  }

  throw JsonValidationError(
      transform,
      /* field */ std::nullopt,
      "Could not be parsed as a valid SourceAsTransform");
}

std::string SourceAsTransform::to_trace_string() const {
  return fmt::format("SourceAsTransform[{}]", source_kind_->to_trace_string());
}

} // namespace marianatrench

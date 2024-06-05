/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string/predicate.hpp>
#include <fmt/format.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/NamedTransform.h>
#include <mariana-trench/SourceAsTransform.h>
#include <mariana-trench/Transform.h>

namespace marianatrench {

const Transform* Transform::from_json(
    const Json::Value& value,
    Context& context) {
  return Transform::from_trace_string(JsonValidation::string(value), context);
}

const Transform* Transform::from_trace_string(
    const std::string& transform,
    Context& context) {
  if (boost::starts_with(transform, "SourceAsTransform[")) {
    return SourceAsTransform::from_trace_string(transform, context);
  }
  // Default to NamedTransform
  return NamedTransform::from_trace_string(transform, context);
}

std::ostream& operator<<(std::ostream& out, const Transform& transform) {
  transform.show(out);
  return out;
}

} // namespace marianatrench

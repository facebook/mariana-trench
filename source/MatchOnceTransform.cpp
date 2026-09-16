/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/MatchOnceTransform.h>
#include <mariana-trench/TransformKind.h>
#include <mariana-trench/TransformList.h>
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {

std::string MatchOnceTransform::to_trace_string() const {
  return "MatchOnce";
}

void MatchOnceTransform::show(std::ostream& out) const {
  out << to_trace_string();
}

const MatchOnceTransform* MatchOnceTransform::from_trace_string(
    const std::string& transform,
    Context& context) {
  if (transform == "MatchOnce") {
    return context.transforms_factory->create_match_once_transform();
  }

  throw JsonValidationError(
      transform,
      /* field */ std::nullopt,
      "Could not be parsed as a valid MatchOnce transform");
}

namespace {

bool contains_match_once(const TransformList* MT_NULLABLE transforms) {
  if (transforms == nullptr) {
    return false;
  }
  return std::any_of(
      transforms->begin(), transforms->end(), [](const Transform* transform) {
        return transform->is<MatchOnceTransform>();
      });
}

} // namespace

bool has_match_once_transform(const Kind* kind) {
  const auto* transform_kind = kind->as<TransformKind>();
  if (transform_kind == nullptr) {
    return false;
  }
  return contains_match_once(transform_kind->local_transforms()) ||
      contains_match_once(transform_kind->global_transforms());
}

} // namespace marianatrench

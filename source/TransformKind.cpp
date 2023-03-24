/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/TransformKind.h>

namespace marianatrench {

bool TransformKind::operator==(const TransformKind& other) const {
  return base_kind_ == other.base_kind_ &&
      local_transforms_ == other.local_transforms_ &&
      global_transforms_ == other.global_transforms_;
}

void TransformKind::show(std::ostream& out) const {
  out << to_trace_string();
}

std::string TransformKind::to_trace_string() const {
  // Expected format for taint transforms:
  // `local_transforms@global_transforms:base_kind`
  std::string value;

  if (local_transforms_ != nullptr) {
    value = local_transforms_->to_trace_string();
    value.append("@");
  }

  if (global_transforms_ != nullptr) {
    value.append(global_transforms_->to_trace_string());
    value.append(":");
  }

  value.append(base_kind_->to_trace_string());

  return value;
}

const Kind* TransformKind::discard_transforms() const {
  return base_kind_;
}

} // namespace marianatrench

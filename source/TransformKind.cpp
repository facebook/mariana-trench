/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/KindFactory.h>
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

const TransformKind* TransformKind::from_trace_string(
    const std::string& kind,
    Context& context) {
  auto local_transforms_separator = kind.find("@");

  const TransformList* local_transforms = nullptr;
  auto remaining_string = kind;
  if (local_transforms_separator != std::string::npos) {
    // Has local transforms (i.e. local@[global:]base_kind)
    auto local_transforms_string =
        remaining_string.substr(0, local_transforms_separator);
    local_transforms = context.transforms_factory->create(
        TransformList::from_trace_string(local_transforms_string, context));

    // Remainder of the string: [global:]base_kind
    remaining_string = kind.substr(local_transforms_separator + 1);
  }

  const TransformList* global_transforms = nullptr;
  auto base_kind_separator = remaining_string.find_last_of(':');
  if (base_kind_separator != std::string::npos) {
    // Has global transforms (i.e. global:base_kind)
    auto global_transforms_string =
        remaining_string.substr(0, base_kind_separator);
    global_transforms = context.transforms_factory->create(
        TransformList::from_trace_string(global_transforms_string, context));

    // Remainder of the string: base_kind
    remaining_string = remaining_string.substr(base_kind_separator + 1);
  }

  const auto* base_kind = Kind::from_trace_string(remaining_string, context);
  return context.kind_factory->transform_kind(
      base_kind, local_transforms, global_transforms);
}

const Kind* TransformKind::discard_transforms() const {
  return base_kind_;
}

} // namespace marianatrench

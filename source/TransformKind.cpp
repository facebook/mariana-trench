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
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {

bool TransformKind::operator==(const TransformKind& other) const {
  return base_kind_ == other.base_kind_ &&
      local_transforms_ == other.local_transforms_ &&
      global_transforms_ == other.global_transforms_;
}

void TransformKind::show(std::ostream& out) const {
  out << to_trace_string();
}

Json::Value TransformKind::to_json() const {
  auto value = Json::Value(Json::objectValue);
  auto inner_value = Json::Value(Json::objectValue);

  if (local_transforms_ != nullptr) {
    inner_value["local"] = local_transforms_->to_trace_string();
  }

  if (global_transforms_ != nullptr) {
    inner_value["global"] = global_transforms_->to_trace_string();
  }

  inner_value["base"] = base_kind_->to_trace_string();

  value["kind"] = inner_value;
  return value;
}

const TransformKind* TransformKind::from_inner_json(
    const Json::Value& value,
    Context& context) {
  // Parse local transforms
  const TransformList* local_transforms = nullptr;
  if (value.isMember("local")) {
    auto local_transforms_string =
        JsonValidation::string(value, /* field */ "local");
    local_transforms = context.transforms_factory->create(
        TransformList::from_trace_string(local_transforms_string, context));
  }

  // Parse global transforms
  const TransformList* global_transforms = nullptr;
  if (value.isMember("global")) {
    auto global_transforms_string =
        JsonValidation::string(value, /* field */ "global");
    global_transforms = context.transforms_factory->create(
        TransformList::from_trace_string(global_transforms_string, context));
  }

  // Parse base kind
  auto base_kind_string = JsonValidation::string(value, "base");
  const auto* base_kind = Kind::from_trace_string(base_kind_string, context);

  return context.kind_factory->transform_kind(
      base_kind, local_transforms, global_transforms);
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

bool TransformKind::has_source_as_transform() const {
  return (local_transforms_ && local_transforms_->has_source_as_transform()) ||
      (global_transforms_ && global_transforms_->has_source_as_transform());
}

bool TransformKind::has_non_sanitize_transform() const {
  return (local_transforms_ &&
          local_transforms_->has_non_sanitize_transform()) ||
      (global_transforms_ && global_transforms_->has_non_sanitize_transform());
}

} // namespace marianatrench

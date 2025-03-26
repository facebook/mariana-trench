/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/PartialKind.h>

namespace marianatrench {

void PartialKind::show(std::ostream& out) const {
  out << "Partial:" << name_ << ":" << label_;
}

Json::Value PartialKind::to_json() const {
  auto nestedValue = Json::Value(Json::objectValue);
  nestedValue["name"] = name_;
  nestedValue["partial_label"] = label_;

  auto value = Json::Value(Json::objectValue);
  value["kind"] = nestedValue;

  return value;
}

const PartialKind* PartialKind::from_json(
    const Json::Value& value,
    Context& context) {
  // Notable asymmetry with to_json():
  // to_json() nests the value in a "kind" field to be consistent with the
  // overridden Kind::to_json().
  // from_json(value, ...) assumes `value` has been extracted from "kind" field.
  auto name = JsonValidation::string(value, /* field */ "name");
  auto label = JsonValidation::string(value, /* field */ "partial_label");
  return context.kind_factory->get_partial(name, label);
}

const PartialKind* PartialKind::from_rule_json(
    const Json::Value& value,
    const std::string& label,
    Context& context) {
  auto name = JsonValidation::string(value);
  return context.kind_factory->get_partial(name, label);
}

std::string PartialKind::to_trace_string() const {
  return "Partial:" + name_ + ":" + label_;
}

bool PartialKind::is_counterpart(const PartialKind* other) const {
  return other->name() == name_ && other->label() != label_;
}

} // namespace marianatrench

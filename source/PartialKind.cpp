/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Kinds.h>
#include <mariana-trench/PartialKind.h>

namespace marianatrench {

void PartialKind::show(std::ostream& out) const {
  out << "Partial:" << name_ << ":" << label_;
}

const PartialKind* PartialKind::from_json(
    const Json::Value& value,
    const std::string& label,
    Context& context) {
  auto name = JsonValidation::string(value);
  return context.kinds->get_partial(name, label);
}

std::string PartialKind::to_trace_string() const {
  return "Partial:" + name_ + ":" + label_;
}

bool PartialKind::is_counterpart(const PartialKind* other) const {
  return other->name() == name_ && other->label() != label_;
}

} // namespace marianatrench

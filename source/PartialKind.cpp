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
  out << "Partial:" << label_ << ":" << name();
}

const PartialKind* PartialKind::from_json(
    const Json::Value& value,
    const std::string& label,
    Context& context) {
  auto name = JsonValidation::string(value);
  return context.kinds->get_partial(name, label);
}

Json::Value PartialKind::to_json() const {
  // TODO(T66517244): Should this be { "name": name(), "label": label() }?
  // Also check what is needed for traces to work..
  auto value = "Partial:" + label() + ":" + name();
  return Json::Value(value);
}

} // namespace marianatrench

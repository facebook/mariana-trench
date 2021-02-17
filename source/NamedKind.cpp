/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Kinds.h>
#include <mariana-trench/NamedKind.h>

namespace marianatrench {

void NamedKind::show(std::ostream& out) const {
  out << name_;
}

const NamedKind* NamedKind::from_json(
    const Json::Value& value,
    Context& context) {
  auto name = JsonValidation::string(value);
  return context.kinds->get(name);
}

Json::Value NamedKind::to_json() const {
  return Json::Value(name_);
}

} // namespace marianatrench

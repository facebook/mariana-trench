/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/CanonicalName.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

CanonicalName CanonicalName::from_json(const Json::Value& value) {
  auto name = JsonValidation::string(value);
  return CanonicalName(name);
}

std::ostream& operator<<(std::ostream& out, const CanonicalName& root) {
  return out << root.value_;
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/MethodSet.h>
#include <mariana-trench/Methods.h>

namespace marianatrench {

MethodSet MethodSet::from_json(const Json::Value& value, Context& context) {
  MethodSet methods;
  for (const auto& method_value : JsonValidation::null_or_array(value)) {
    methods.add(Method::from_json(method_value, context));
  }
  return methods;
}

Json::Value MethodSet::to_json() const {
  auto methods = Json::Value(Json::arrayValue);
  for (const auto* method : set_) {
    methods.append(method->to_json());
  }
  return methods;
}

std::ostream& operator<<(std::ostream& out, const MethodSet& methods) {
  if (methods.is_top()) {
    return out << "T";
  }
  out << "{";
  for (auto iterator = methods.begin(), end = methods.end(); iterator != end;) {
    out << "`" << show(*iterator) << "`";
    ++iterator;
    if (iterator != end) {
      out << ", ";
    }
  }
  return out << "}";
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/OriginSet.h>

namespace marianatrench {

Json::Value OriginSet::to_json() const {
  auto origins = Json::Value(Json::arrayValue);
  for (const auto* origin : set_) {
    origins.append(origin->to_json());
  }
  return origins;
}

std::ostream& operator<<(std::ostream& out, const OriginSet& origins) {
  out << "{";
  for (auto iterator = origins.begin(), end = origins.end(); iterator != end;) {
    out << "`" << (*iterator)->to_string() << "`";
    ++iterator;
    if (iterator != end) {
      out << ", ";
    }
  }
  return out << "}";
}

} // namespace marianatrench

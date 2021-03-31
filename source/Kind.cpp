/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Kind.h>

namespace marianatrench {

Json::Value Kind::to_json() const {
  auto value = Json::Value(Json::objectValue);
  value["kind"] = to_trace_string();
  return value;
}

std::ostream& operator<<(std::ostream& out, const Kind& kind) {
  kind.show(out);
  return out;
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/Kinds.h>

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

const Kind* Kind::from_json(const Json::Value& value, Context& context) {
  const auto leaf_kind = JsonValidation::string(value, /* field */ "kind");
  if (value.isMember("partial_label")) {
    return context.kinds->get_partial(
        leaf_kind, JsonValidation::string(value, /* field */ "partial_label"));
  } else {
    return context.kinds->get(leaf_kind);
  }
}

} // namespace marianatrench

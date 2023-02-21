/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Assert.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/LocalPositionSet.h>
#include <mariana-trench/Positions.h>

namespace marianatrench {

LocalPositionSet LocalPositionSet::from_json(
    const Json::Value& value,
    Context& context) {
  LocalPositionSet set;
  for (const auto& position_value : JsonValidation::null_or_array(value)) {
    set.add(Position::from_json(position_value, context));
  }
  return set;
}

Json::Value LocalPositionSet::to_json() const {
  mt_assert(!is_bottom());
  auto lines = Json::Value(Json::arrayValue);
  if (set_.is_value()) {
    for (const auto* position : set_.elements()) {
      lines.append(position->to_json(/* with_path */ false));
    }
  }
  return lines;
}

std::ostream& operator<<(std::ostream& out, const LocalPositionSet& positions) {
  return out << positions.set_;
}

} // namespace marianatrench

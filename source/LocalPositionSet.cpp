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

LocalPositionSet::LocalPositionSet(
    std::initializer_list<const Position*> positions)
    : set_(positions) {}

void LocalPositionSet::add(const Position* position) {
  set_.add(position);
}

bool LocalPositionSet::leq(const LocalPositionSet& other) const {
  return set_.leq(other.set_);
}

bool LocalPositionSet::equals(const LocalPositionSet& other) const {
  return set_.equals(other.set_);
}

void LocalPositionSet::join_with(const LocalPositionSet& other) {
  set_.join_with(other.set_);
}

void LocalPositionSet::widen_with(const LocalPositionSet& other) {
  set_.widen_with(other.set_);
}

void LocalPositionSet::meet_with(const LocalPositionSet& other) {
  set_.meet_with(other.set_);
}

void LocalPositionSet::narrow_with(const LocalPositionSet& other) {
  set_.narrowing(other.set_);
}

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

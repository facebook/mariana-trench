/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/AliasingProperties.h>

namespace marianatrench {

bool AliasingProperties::leq(const AliasingProperties& other) const {
  if (is_bottom()) {
    return true;
  } else if (other.is_bottom()) {
    return false;
  }
  return local_positions_.leq(other.local_positions_) &&
      locally_inferred_features_.leq(other.locally_inferred_features_);
}

bool AliasingProperties::equals(const AliasingProperties& other) const {
  return local_positions_.equals(other.local_positions_) &&
      locally_inferred_features_.equals(other.locally_inferred_features_);
}

void AliasingProperties::join_with(const AliasingProperties& other) {
  mt_if_expensive_assert(auto previous = *this);

  local_positions_.join_with(other.local_positions_);
  locally_inferred_features_.join_with(other.locally_inferred_features_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void AliasingProperties::widen_with(const AliasingProperties& other) {
  mt_if_expensive_assert(auto previous = *this);

  local_positions_.widen_with(other.local_positions_);
  locally_inferred_features_.widen_with(other.locally_inferred_features_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void AliasingProperties::meet_with(const AliasingProperties& other) {
  local_positions_.meet_with(other.local_positions_);
  locally_inferred_features_.meet_with(other.locally_inferred_features_);
}

void AliasingProperties::narrow_with(const AliasingProperties& other) {
  local_positions_.narrow_with(other.local_positions_);
  locally_inferred_features_.narrow_with(other.locally_inferred_features_);
}

void AliasingProperties::difference_with(const AliasingProperties& other) {
  if (local_positions_.leq(other.local_positions_)) {
    local_positions_ = {};
  }
  if (locally_inferred_features_.leq(other.locally_inferred_features_)) {
    locally_inferred_features_.set_to_bottom();
  }
}

void AliasingProperties::add_local_position(const Position* position) {
  local_positions_.add(position);
}

void AliasingProperties::add_locally_inferred_features(
    const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  locally_inferred_features_.add(features);
}

std::ostream& operator<<(
    std::ostream& out,
    const AliasingProperties& properties) {
  mt_assert(!properties.is_top());
  return out << "AliasingProperties(local_positions="
             << properties.local_positions_ << ", locally_inferred_features="
             << properties.locally_inferred_features_ << ")";
}

} // namespace marianatrench

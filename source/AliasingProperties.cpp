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
      locally_inferred_features_.leq(other.locally_inferred_features_) &&
      collapse_depth_.leq(other.collapse_depth_);
}

bool AliasingProperties::equals(const AliasingProperties& other) const {
  return local_positions_.equals(other.local_positions_) &&
      locally_inferred_features_.equals(other.locally_inferred_features_) &&
      collapse_depth_.equals(other.collapse_depth_);
}

void AliasingProperties::join_with(const AliasingProperties& other) {
  mt_if_expensive_assert(auto previous = *this);

  local_positions_.join_with(other.local_positions_);
  locally_inferred_features_.join_with(other.locally_inferred_features_);
  collapse_depth_.join_with(other.collapse_depth_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void AliasingProperties::widen_with(const AliasingProperties& other) {
  mt_if_expensive_assert(auto previous = *this);

  local_positions_.widen_with(other.local_positions_);
  locally_inferred_features_.widen_with(other.locally_inferred_features_);
  collapse_depth_.widen_with(other.collapse_depth_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void AliasingProperties::meet_with(const AliasingProperties& other) {
  local_positions_.meet_with(other.local_positions_);
  locally_inferred_features_.meet_with(other.locally_inferred_features_);
  collapse_depth_.meet_with(other.collapse_depth_);
}

void AliasingProperties::narrow_with(const AliasingProperties& other) {
  local_positions_.narrow_with(other.local_positions_);
  locally_inferred_features_.narrow_with(other.locally_inferred_features_);
  collapse_depth_.narrow_with(other.collapse_depth_);
}

void AliasingProperties::difference_with(const AliasingProperties& other) {
  if (local_positions_.leq(other.local_positions_)) {
    local_positions_ = {};
  }
  if (locally_inferred_features_.leq(other.locally_inferred_features_)) {
    locally_inferred_features_ = {};
  }
  collapse_depth_.difference_with(other.collapse_depth_);
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

void AliasingProperties::set_always_collapse() {
  collapse_depth_ = CollapseDepth::collapse();
}

std::ostream& operator<<(
    std::ostream& out,
    const AliasingProperties& properties) {
  mt_assert(!properties.is_top());
  return out << "AliasingProperties(local_positions="
             << properties.local_positions_ << ", locally_inferred_features="
             << properties.locally_inferred_features_
             << ", collapse_depth=" << properties.collapse_depth_ << ")";
}

} // namespace marianatrench

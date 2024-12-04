/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/PointsToSet.h>

namespace marianatrench {

PointsToSet::PointsToSet(RootMemoryLocation* memory_location) {
  set_internal(memory_location, AliasingProperties::empty());
}

PointsToSet::PointsToSet(
    RootMemoryLocation* memory_location,
    AliasingProperties properties) {
  set_internal(memory_location, std::move(properties));
}

PointsToSet::PointsToSet(
    std::initializer_list<RootMemoryLocation*> memory_locations) {
  for (auto* memory_location : memory_locations) {
    set_internal(memory_location, AliasingProperties::empty());
  }
}

PointsToSet::PointsToSet(const MemoryLocationsDomain& memory_locations) {
  for (auto* memory_location : memory_locations) {
    auto* root_memory_location = memory_location->as<RootMemoryLocation>();
    mt_assert(root_memory_location != nullptr);
    set_internal(root_memory_location, AliasingProperties::empty());
  }
}

void PointsToSet::add_local_position(const Position* position) {
  map_.transform([position](AliasingProperties properties) {
    properties.add_local_position(position);
    return properties;
  });
}

void PointsToSet::add_locally_inferred_features(
    const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  map_.transform([&features](AliasingProperties properties) {
    properties.add_locally_inferred_features(features);
    return properties;
  });
}

std::ostream& operator<<(std::ostream& out, const PointsToSet& points_to_set) {
  mt_assert(!points_to_set.is_top());

  out << "PointsToSet{";
  for (const auto& [memory_location, properties] :
       points_to_set.map_.bindings()) {
    out << show(memory_location) << " -> " << properties << ", ";
  }

  return out << "}";
}

} // namespace marianatrench

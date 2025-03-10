/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Log.h>
#include <mariana-trench/PointsToEnvironment.h>
#include <mariana-trench/WideningPointsToResolver.h>

namespace marianatrench {

WideningPointsToResolver PointsToEnvironment::make_widening_resolver() const {
  return WideningPointsToResolver{*this};
}

PointsToSet PointsToEnvironment::points_to(
    const MemoryLocationsDomain& memory_locations) const {
  PointsToSet result{};
  for (auto* memory_location : memory_locations) {
    result.join_with(points_to(memory_location));
  }
  return result;
}

PointsToSet PointsToEnvironment::points_to(
    MemoryLocation* memory_location) const {
  if (auto* root_memory_location = memory_location->as<RootMemoryLocation>()) {
    return PointsToSet{root_memory_location};
  }

  auto root_points_to_tree = environment_.get(memory_location->root());
  if (root_points_to_tree.is_bottom()) {
    // No aliases.
    return PointsToSet::bottom();
  }

  auto [remaining_path, points_to_tree] =
      root_points_to_tree.raw_read_max_path(memory_location->path());

  if (remaining_path.empty() || points_to_tree.is_bottom()) {
    return points_to_tree.root();
  }

  PointsToSet result{};
  for (const auto& [target_memory_location, _properties] :
       points_to_tree.root()) {
    // This recursion is safe because points-to tree does not store field memory
    // locations. Hence the remaining path is always shorter than the
    // original path. i.e. the depth of the recursion is limited by the length
    // of the path of the field memory location for which we are computing the
    // points-to set.
    result.join_with(
        points_to(target_memory_location->make_field(remaining_path)));
  }

  return result;
}

void PointsToEnvironment::write(
    const WideningPointsToResolver& widening_resolver,
    MemoryLocation* memory_location,
    const DexString* field,
    const PointsToSet& points_tos,
    UpdateKind kind) {
  PointsToSet target_memory_locations;
  Path full_path{};

  if (auto* root_memory_location = memory_location->as<RootMemoryLocation>()) {
    target_memory_locations = PointsToSet{root_memory_location};
  } else {
    // Resolve aliases to find the memory locations to update
    auto resolved_aliases =
        widening_resolver.resolved_aliases(memory_location->root());
    auto [remaining_path, target_points_to_tree] =
        resolved_aliases.raw_read_max_path(memory_location->path());

    target_memory_locations = target_points_to_tree.root();
    full_path = std::move(remaining_path);
  }

  full_path.append(PathElement::field(field));

  if (kind == UpdateKind::Strong && target_memory_locations.size() > 1) {
    // In practice, only one of the memory location is affected, so we must
    // treat this as a weak update, even if a strong update was requested.
    kind = UpdateKind::Weak;
  }

  for (const auto& [target_memory_location, _properties] :
       target_memory_locations) {
    environment_.update(
        target_memory_location,
        [&full_path, &points_tos, kind](const PointsToTree& tree) {
          auto copy = tree;
          // Wrap with a PointsToTree to break aliases (i.e. discard
          // previous subtree (if any)) under this node when UpdateKind is
          // Strong.
          copy.write(full_path, PointsToTree{points_tos}, kind);
          return copy;
        });
  }
}

std::ostream& operator<<(
    std::ostream& out,
    const PointsToEnvironment& environment) {
  if (environment.is_bottom()) {
    return out << "_|_";
  } else if (environment.is_top()) {
    return out << "T";
  } else {
    out << "PointsToEnvironment(";
    for (const auto& entry : environment.environment_.bindings()) {
      out << "\n  " << show(entry.first) << " -> " << entry.second;
    }
    return out << "\n)";
  }
}

} // namespace marianatrench

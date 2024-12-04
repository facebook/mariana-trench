/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/TaintEnvironment.h>

namespace marianatrench {

void TaintEnvironment::write(
    MemoryLocation* memory_location,
    const Path& path,
    Taint taint,
    UpdateKind kind) {
  Path full_path = memory_location->path();
  full_path.extend(path);

  environment_.update(
      memory_location->root(),
      [&full_path, &taint, kind](const AbstractTaintTree& tree) {
        auto copy = tree;
        copy.write_taint_tree(full_path, TaintTree{std::move(taint)}, kind);
        return copy;
      });
}

void TaintEnvironment::write(
    MemoryLocation* memory_location,
    const Path& path,
    TaintTree taint,
    UpdateKind kind) {
  Path full_path = memory_location->path();
  full_path.extend(path);

  environment_.update(
      memory_location->root(),
      [&full_path, &taint, kind](const AbstractTaintTree& tree) {
        auto copy = tree;
        copy.write_taint_tree(full_path, std::move(taint), kind);
        return copy;
      });
}

PointsToSet TaintEnvironment::points_to(
    const MemoryLocationsDomain& memory_locations) const {
  PointsToSet result{};
  for (auto* memory_location : memory_locations) {
    result.join_with(points_to(memory_location));
  }
  return result;
}

PointsToSet TaintEnvironment::points_to(MemoryLocation* memory_location) const {
  if (auto* root_memory_location = memory_location->as<RootMemoryLocation>()) {
    return PointsToSet{root_memory_location};
  }

  auto root_points_to_tree =
      environment_.get(memory_location->root()).aliases();
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

PointsToTree TaintEnvironment::resolve_aliases(
    RootMemoryLocation* root_memory_location) const {
  PointsToTree resolved_aliases{};
  // Track visited to detect back edges and avoid infinite loops.
  std::unordered_set<MemoryLocation*> visited{};

  LOG(5,
      "Resolving points-to tree for root memory location: {}",
      show(root_memory_location));

  resolve_aliases_internal(
      root_memory_location,
      Path{},
      AliasingProperties::empty(),
      resolved_aliases,
      visited);

  LOG(5,
      "Resolved points-to tree for root memory location: {} is: {}",
      show(root_memory_location),
      resolved_aliases);

  return resolved_aliases;
}

void TaintEnvironment::resolve_aliases_internal(
    RootMemoryLocation* memory_location,
    const Path& path,
    const AliasingProperties& aliasing_properties,
    PointsToTree& resolved_aliases,
    std::unordered_set<MemoryLocation*>& visited) const {
  if (visited.count(memory_location) > 0) {
    LOG(5,
        "Found loop while resolving points-to tree at {} back to: {}",
        path,
        show(memory_location));
    // TODO: T142954672 Widen when a loop is found. Currently we just
    // break the cycle resulting in FN.
    return;
  }

  LOG(5,
      "Visiting memory location {} with remaining path {}.",
      show(memory_location),
      path);
  visited.insert(memory_location);

  resolved_aliases.write(
      path,
      PointsToSet{memory_location, aliasing_properties},
      UpdateKind::Weak);

  auto points_to_tree = environment_.get(memory_location).aliases();

  if (points_to_tree.is_bottom()) {
    visited.erase(memory_location);
    return;
  }

  points_to_tree.visit(
      [this, &resolved_aliases, &path, &visited](
          const Path& inner_path, const PointsToSet& points_to_set) {
        // The root element of the PointsToTree of a root memory location is
        // always empty.
        mt_assert(!inner_path.empty() || points_to_set.is_bottom());
        for (const auto& [points_to, properties] : points_to_set) {
          // Compute the full path for the resolved_aliases tree.
          Path full_path = path;
          full_path.extend(inner_path);
          this->resolve_aliases_internal(
              points_to, full_path, properties, resolved_aliases, visited);
        }
      });

  LOG(5,
      "Resolved points-to tree after visiting: {} is: {}",
      show(memory_location),
      resolved_aliases);

  visited.erase(memory_location);
}

void TaintEnvironment::write(
    MemoryLocation* memory_location,
    const DexString* field,
    const PointsToSet& points_tos,
    UpdateKind kind) {
  LOG(5,
      "{} update points-to tree at: {} field `{}` with {}",
      kind == UpdateKind::Strong ? "Strong" : "Weak",
      show(memory_location),
      field->str(),
      points_tos);

  // Resolve aliases to find the memory locations to update
  auto resolved_aliases = resolve_aliases(memory_location->root());

  auto [remaining_path, target_memory_locations] =
      resolved_aliases.raw_read_max_path(memory_location->path());

  Path full_path = remaining_path;
  full_path.append(PathElement::field(field));

  for (const auto& [target_memory_location, _properties] :
       target_memory_locations.root()) {
    LOG(5,
        "{} updating points-to tree of {}. Root: {}, {} with: {}",
        kind == UpdateKind::Strong ? "Strong" : "Weak",
        show(target_memory_location),
        show(target_memory_location->root()),
        full_path,
        points_tos);

    environment_.update(
        target_memory_location->root(),
        [&full_path, &points_tos, kind](const AbstractTaintTree& tree) {
          auto copy = tree;
          // Wrap with a PointsToTree to break aliases (i.e. discard
          // previous subtree (if any)) under this node when UpdateKind is
          // Strong.
          copy.write_alias_tree(full_path, PointsToTree{points_tos}, kind);
          return copy;
        });
  }

  LOG(5,
      "Updated points-to tree at: {} is: {}",
      show(memory_location),
      points_to(memory_location));
}

std::ostream& operator<<(
    std::ostream& out,
    const TaintEnvironment& environment) {
  if (environment.is_bottom()) {
    return out << "_|_";
  } else if (environment.is_top()) {
    return out << "T";
  } else {
    out << "TaintEnvironment(";
    for (const auto& entry : environment.environment_.bindings()) {
      out << "\n  " << show(entry.first) << " -> " << entry.second;
    }
    return out << "\n)";
  }
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Log.h>
#include <mariana-trench/PointsToEnvironment.h>

namespace marianatrench {

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

PointsToTree PointsToEnvironment::resolve_aliases(
    RootMemoryLocation* root_memory_location) const {
  PointsToTree resolved_aliases{};
  // Track visited to detect back edges and avoid infinite loops.
  std::unordered_set<MemoryLocation*> visited{};

  resolve_aliases_internal(
      root_memory_location,
      Path{},
      AliasingProperties::empty(),
      resolved_aliases,
      visited);

  return resolved_aliases;
}

void PointsToEnvironment::resolve_aliases_internal(
    RootMemoryLocation* memory_location,
    const Path& path,
    const AliasingProperties& aliasing_properties,
    PointsToTree& resolved_aliases,
    std::unordered_set<MemoryLocation*>& visited) const {
  if (visited.count(memory_location) > 0) {
    WARNING(
        5,
        "Found loop while resolving points-to tree at {} back to: {}",
        path,
        show(memory_location));
    // TODO: T142954672 Widen when a loop is found. Currently we just
    // break the cycle resulting in FN.
    return;
  }

  visited.insert(memory_location);

  resolved_aliases.write(
      path,
      PointsToSet{memory_location, aliasing_properties},
      UpdateKind::Weak);

  auto points_to_tree = environment_.get(memory_location);

  if (points_to_tree.is_bottom()) {
    // Remove memory_location from visited there is no subtree to resolve.
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

  // Remove memory_location from visited as we resolved its subtree.
  visited.erase(memory_location);
}

void PointsToEnvironment::write(
    MemoryLocation* memory_location,
    const DexString* field,
    const PointsToSet& points_tos,
    UpdateKind kind) {
  // Resolve aliases to find the memory locations to update
  auto resolved_aliases = resolve_aliases(memory_location->root());

  auto [remaining_path, target_memory_locations] =
      resolved_aliases.raw_read_max_path(memory_location->path());

  if (kind == UpdateKind::Strong && target_memory_locations.root().size() > 1) {
    // In practice, only one of the memory location is affected, so we must
    // treat this as a weak update, even if a strong update was requested.
    kind = UpdateKind::Weak;
  }

  Path full_path = remaining_path;
  full_path.append(PathElement::field(field));

  for (const auto& [target_memory_location, _properties] :
       target_memory_locations.root()) {
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

ResolvedAliasesMap ResolvedAliasesMap::from_environments(
    const Method* method,
    MemoryFactory& memory_factory,
    const MemoryLocationEnvironment& memory_locations_environment,
    const PointsToEnvironment& points_to_environment,
    const IRInstruction* instruction) {
  LOG(5,
      "Building ResolvedAliasesMap for instruction `{}` from points-to environment: {}",
      show(instruction),
      points_to_environment);

  Map result{};

  for (Register register_id : instruction->srcs()) {
    for (auto* source_memory_location :
         memory_locations_environment.get(register_id)) {
      if (result.find(source_memory_location->root()) != result.end()) {
        continue;
      }

      auto points_to_tree =
          points_to_environment.resolve_aliases(source_memory_location->root());

      result.insert_or_assign(source_memory_location->root(), points_to_tree);
    }
  }

  if (!method->is_static() && opcode::is_a_return(instruction->opcode())) {
    // analyze return infers generations on the `this` parameter so we need to
    // provide the memory locations and the associated resolved points-to tree.
    auto* this_memory_location = memory_factory.make_parameter(0);
    auto points_to_tree =
        points_to_environment.resolve_aliases(this_memory_location);

    result.insert_or_assign(this_memory_location, points_to_tree);
  }

  return ResolvedAliasesMap{std::move(result)};
}

PointsToTree ResolvedAliasesMap::get(
    RootMemoryLocation* root_memory_location) const {
  auto it = map_.find(root_memory_location);
  if (it == map_.end()) {
    LOG(4,
        "No resolved aliases for root memory location `{}`",
        show(root_memory_location));

    // When no aliases (i.e. points-to tree) is present, the memory location
    // resolves to itself.
    return PointsToTree{PointsToSet{root_memory_location}};
  }

  return it->second;
}

} // namespace marianatrench

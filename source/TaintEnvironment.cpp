/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/AliasingProperties.h>
#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/TaintEnvironment.h>

namespace marianatrench {

TaintTree TaintEnvironment::read(MemoryLocation* memory_location) const {
  return environment_.get(memory_location->root())
      .read(memory_location->path());
}

TaintTree TaintEnvironment::read(
    MemoryLocation* memory_location,
    const Path& path) const {
  Path full_path = memory_location->path();
  full_path.extend(path);

  return environment_.get(memory_location->root()).read(full_path);
}

TaintTree TaintEnvironment::read(
    const MemoryLocationsDomain& memory_locations) const {
  TaintTree taint;
  for (auto* memory_location : memory_locations.elements()) {
    taint.join_with(read(memory_location));
  }
  return taint;
}

TaintTree TaintEnvironment::read(
    const MemoryLocationsDomain& memory_locations,
    const Path& path) const {
  TaintTree taint;
  for (auto* memory_location : memory_locations.elements()) {
    taint.join_with(read(memory_location, path));
  }

  return taint;
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
      [&full_path, &taint, kind](const TaintTree& tree) {
        auto copy = tree;
        copy.write(full_path, std::move(taint), kind);
        return copy;
      });
}

void TaintEnvironment::write(
    const MemoryLocationsDomain& memory_locations,
    const Path& path,
    TaintTree taint,
    UpdateKind kind) {
  if (memory_locations.empty()) {
    return;
  }

  if (kind == UpdateKind::Strong && memory_locations.singleton() == nullptr) {
    // In practice, only one of the memory location is affected, so we must
    // treat this as a weak update, even if a strong update was requested.
    kind = UpdateKind::Weak;
  }

  for (auto* memory_location : memory_locations.elements()) {
    write(memory_location, path, taint, kind);
  }
}

TaintTree TaintEnvironment::deep_read(
    const WideningPointsToResolver& widening_resolver,
    MemoryLocation* memory_location) const {
  TaintTree result{};

  // Retrieve the fully resolved PointsToTree for the RootMemoryLocation.
  // It is necessary to build the full taint tree for a deep read even if we are
  // reading taint at a certain path. This is because the TaintTree at a memory
  // location higher in the points-to tree could have overlapping paths with the
  // current points-to tree which needs to be joined for the full deep read.
  auto points_to_tree =
      widening_resolver.resolved_aliases(memory_location->root());

  // To build the complete taint tree for the RootMemoryLocation, we visit the
  // fully resolved points-to-tree in postorder. This is necessary for
  // correctness and accuracy of the final taint tree.
  //
  // Specifically, this is necessary to correctly apply the collapse-depth when
  // reading the taint from the pointee memory location. Note that nodes of the
  // points-to-tree are "self-contained" i.e. we do not propagate information
  // from the ancestors down to the children.
  //
  // Eg: Say we have a collapse-depth=always at the root node. For correctness,
  // we need to build the complete taint tree for all the children and then
  // collapse the entire taint tree. Doing an inorder traversal would lead
  // incorrect results.
  points_to_tree.visit_postorder([this, &result](
                                     const Path& path,
                                     const PointsToSet& points_to_set) {
    // When we read the taint from the pointee memory location, we need to
    // apply the aliasing properties to the taint i.e. add the local
    // positions, features, and collapse the taint read from that memory
    // location before adding it to the final result taint.

    if (path.empty()) {
      // - Empty path implies that this is the root node of the PointsToTree
      // i.e. the PointsToSet here contains the self-resolution and is only
      // present in the fully resolved PointsToTree retrieved from the
      // WideningPointsToResolver::resolved_aliases().
      // - Hence, for self-resolution, the AliasingProperties is applicable
      // to the whole result taint.

      // For self-resolution, we expect a single points-to memory location
      // in the points_to_set.
      mt_assert(points_to_set.size() == 1);
      const auto& [points_to, properties] = *points_to_set.begin();
      // The pointee memory location in the points_to_set is either:
      // 1. The input `memory_location` itself: In this case, the
      // AliasingProperties is empty.
      // 2. The head of the widened component of which the input
      // memory_location is a part. In this case, the AliasingProperties
      // is non-empty and we expect the collapse-depth to be set to
      // always-collapse.
      mt_assert(
          properties.is_empty() ||
          properties.collapse_depth().is(CollapseDepth::Enum::AlwaysCollapse));

      result.join_with(this->read(points_to));
      result.apply_aliasing_properties(properties);

      if (properties.collapse_depth().should_collapse()) {
        result.collapse_deeper_than(
            /* height */ properties.collapse_depth().value(),
            FeatureMayAlwaysSet{
                FeatureFactory::singleton().get_alias_broadening_feature()});
      }

      return;
    }

    mt_assert(!path.empty());

    // When path is non-empty: the aliasing properties applies to
    // the taint read from the original memory location then written to
    // result taint at path.
    CollapseDepth min_collapse_depth;
    for (const auto& [points_to, properties] : points_to_set) {
      // Read the taint from the points_to memory location
      auto taint = this->read(points_to);

      // Apply the aliasing properties to the taint i.e. add the local
      // positions, features, and collapse the taint read from the memory
      // location to the expected depth.
      taint.apply_aliasing_properties(properties);
      min_collapse_depth.join_with(properties.collapse_depth());

      result.write(path, std::move(taint), UpdateKind::Weak);
    }

    if (min_collapse_depth.should_collapse()) {
      result.collapse_deeper_than(
          path,
          /* height */ min_collapse_depth.value(),
          FeatureMayAlwaysSet{
              FeatureFactory::singleton().get_alias_broadening_feature()});
    }
  });

  // If the input memory_location had a path, we return the sub-tree of the
  // fully resolved result taint tree.
  return result.read(memory_location->path());
}

TaintTree TaintEnvironment::deep_read(
    const WideningPointsToResolver& widening_resolver,
    const MemoryLocationsDomain& memory_locations) const {
  TaintTree result{};
  for (auto* memory_location : memory_locations) {
    result.join_with(deep_read(widening_resolver, memory_location));
  }
  return result;
}

TaintTree TaintEnvironment::deep_read(
    const WideningPointsToResolver& widening_resolver,
    const MemoryLocationsDomain& memory_locations,
    const Path& path) const {
  TaintTree result{};
  for (auto* memory_location : memory_locations) {
    result.join_with(deep_read(widening_resolver, memory_location).read(path));
  }
  return result;
}

void TaintEnvironment::deep_write(
    const WideningPointsToResolver& widening_resolver,
    MemoryLocation* memory_location,
    const Path& path,
    TaintTree taint,
    UpdateKind kind) {
  const auto& points_to_tree =
      widening_resolver.resolved_aliases(memory_location->root());

  auto full_path = memory_location->path();
  full_path.extend(path);

  LOG(5,
      "{} update taint tree at: {} path `{}` with {}",
      kind == UpdateKind::Strong ? "Strong" : "Weak",
      show(memory_location->root()),
      full_path,
      taint);

  // Manually destructuring a pair as cpp17 doesn't allow us to capture
  // destructured variables in lambda below.
  auto raw_read_result = points_to_tree.raw_read_max_path(full_path);
  auto remaining_path = raw_read_result.first;
  auto target_memory_locations = raw_read_result.second.root();

  if (kind == UpdateKind::Strong && target_memory_locations.size() > 1) {
    // In practice, only one of the memory location is affected, so we must
    // treat this as a weak update, even if a strong update was requested.
    kind = UpdateKind::Weak;
  }

  for (const auto& [target_memory_location, properties] :
       target_memory_locations) {
    auto target_update_kind = properties.is_widened() ? UpdateKind::Weak : kind;
    auto taint_to_write = taint;
    taint_to_write.apply_aliasing_properties(properties);

    LOG(5,
        "{} updating taint tree of {} at {} with: {}",
        target_update_kind == UpdateKind::Strong ? "Strong" : "Weak",
        show(target_memory_location),
        remaining_path,
        taint_to_write);

    environment_.update(
        target_memory_location,
        [&remaining_path, &taint_to_write, target_update_kind](
            const TaintTree& tree) {
          auto copy = tree;
          copy.write(
              remaining_path, std::move(taint_to_write), target_update_kind);
          return copy;
        });
  }
}

void TaintEnvironment::deep_write(
    const WideningPointsToResolver& widening_resolver,
    const MemoryLocationsDomain& memory_locations,
    const Path& path,
    TaintTree taint,
    UpdateKind kind) {
  if (memory_locations.empty()) {
    return;
  }

  if (kind == UpdateKind::Strong && memory_locations.singleton() == nullptr) {
    // In practice, only one of the memory location is affected, so we must
    // treat this as a weak update, even if a strong update was requested.
    kind = UpdateKind::Weak;
  }

  for (auto* memory_location : memory_locations.elements()) {
    deep_write(widening_resolver, memory_location, path, taint, kind);
  }
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

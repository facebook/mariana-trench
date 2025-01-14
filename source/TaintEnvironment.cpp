/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/AliasingProperties.h>
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
    const ResolvedAliasesMap& resolved_aliases,
    MemoryLocation* memory_location) const {
  TaintTree result{};
  auto points_to_tree = resolved_aliases.get(memory_location->root());
  points_to_tree.visit(
      [this, &result](const Path& path, const PointsToSet& points_to_set) {
        for (const auto& [points_to, properties] : points_to_set) {
          auto taint = this->read(points_to);
          taint.apply_aliasing_properties(properties);
          result.write(path, std::move(taint), UpdateKind::Weak);
        }
      });

  return result.read(memory_location->path());
}

TaintTree TaintEnvironment::deep_read(
    const ResolvedAliasesMap& resolved_aliases,
    const MemoryLocationsDomain& memory_locations) const {
  TaintTree result{};
  for (auto* memory_location : memory_locations) {
    result.join_with(deep_read(resolved_aliases, memory_location));
  }
  return result;
}

TaintTree TaintEnvironment::deep_read(
    const ResolvedAliasesMap& resolved_aliases,
    const MemoryLocationsDomain& memory_locations,
    const Path& path) const {
  TaintTree result{};
  for (auto* memory_location : memory_locations) {
    result.join_with(deep_read(resolved_aliases, memory_location).read(path));
  }
  return result;
}

void TaintEnvironment::deep_write(
    const ResolvedAliasesMap& resolved_aliases,
    MemoryLocation* memory_location,
    const Path& path,
    TaintTree taint,
    UpdateKind kind) {
  const auto& points_to_tree = resolved_aliases.get(memory_location->root());

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
  auto target_memory_locations = raw_read_result.second;

  if (kind == UpdateKind::Strong && target_memory_locations.root().size() > 1) {
    // In practice, only one of the memory location is affected, so we must
    // treat this as a weak update, even if a strong update was requested.
    kind = UpdateKind::Weak;
  }

  for (const auto& [target_memory_location, properties] :
       target_memory_locations.root()) {
    auto taint_to_write = taint;
    taint_to_write.apply_aliasing_properties(properties);

    LOG(5,
        "{} updating taint tree of {} at {} with: {}",
        kind == UpdateKind::Strong ? "Strong" : "Weak",
        show(target_memory_location),
        remaining_path,
        taint_to_write);

    environment_.update(
        target_memory_location,
        [&remaining_path, &taint_to_write, kind](const TaintTree& tree) {
          auto copy = tree;
          copy.write(remaining_path, std::move(taint_to_write), kind);
          return copy;
        });
  }
}

void TaintEnvironment::deep_write(
    const ResolvedAliasesMap& resolved_aliases,
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
    deep_write(resolved_aliases, memory_location, path, taint, kind);
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

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/ForwardTaintEnvironment.h>

namespace marianatrench {

ForwardTaintEnvironment ForwardTaintEnvironment::initial() {
  return ForwardTaintEnvironment::bottom();
}

TaintTree ForwardTaintEnvironment::read(MemoryLocation* memory_location) const {
  return environment_.get(memory_location->root())
      .taint()
      .read(memory_location->path());
}

TaintTree ForwardTaintEnvironment::read(
    MemoryLocation* memory_location,
    const Path& path) const {
  Path full_path = memory_location->path();
  full_path.extend(path);

  return environment_.get(memory_location->root()).taint().read(full_path);
}

TaintTree ForwardTaintEnvironment::read(
    const MemoryLocationsDomain& memory_locations) const {
  TaintTree taint;
  for (auto* memory_location : memory_locations.elements()) {
    taint.join_with(read(memory_location));
  }
  return taint;
}

TaintTree ForwardTaintEnvironment::read(
    const MemoryLocationsDomain& memory_locations,
    const Path& path) const {
  TaintTree taint;
  for (auto* memory_location : memory_locations.elements()) {
    taint.join_with(read(memory_location, path));
  }

  return taint;
}

void ForwardTaintEnvironment::write(
    MemoryLocation* memory_location,
    TaintTree taint,
    UpdateKind kind) {
  environment_.write(memory_location, Path{}, std::move(taint), kind);
}

void ForwardTaintEnvironment::write(
    MemoryLocation* memory_location,
    const Path& path,
    TaintTree taint,
    UpdateKind kind) {
  environment_.write(memory_location, path, std::move(taint), kind);
}

void ForwardTaintEnvironment::write(
    MemoryLocation* memory_location,
    const Path& path,
    Taint taint,
    UpdateKind kind) {
  environment_.write(memory_location, path, std::move(taint), kind);
}

void ForwardTaintEnvironment::write(
    const MemoryLocationsDomain& memory_locations,
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
    write(memory_location, taint, kind);
  }
}

void ForwardTaintEnvironment::write(
    const MemoryLocationsDomain& memory_locations,
    Taint taint,
    UpdateKind kind) {
  write(memory_locations, TaintTree(std::move(taint)), kind);
}

void ForwardTaintEnvironment::write(
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

void ForwardTaintEnvironment::write(
    const MemoryLocationsDomain& memory_locations,
    const Path& path,
    Taint taint,
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

PointsToSet ForwardTaintEnvironment::points_to(
    MemoryLocation* memory_location) const {
  auto points_tos = environment_.points_to(memory_location);

  LOG(5,
      "Resolved points-to for memory location {} to {}",
      show(memory_location),
      points_tos);

  return points_tos;
}

PointsToSet ForwardTaintEnvironment::points_to(
    const MemoryLocationsDomain& memory_locations) const {
  auto points_tos = environment_.points_to(memory_locations);
  LOG(5,
      "Resolved points-to for memory location {} to {}",
      show(memory_locations),
      points_tos);
  return points_tos;
}

void ForwardTaintEnvironment::write(
    MemoryLocation* memory_location,
    const DexString* field,
    const PointsToSet& points_tos,
    UpdateKind kind) {
  LOG(5,
      "{} updating PointsToTree at {} with {}",
      kind == UpdateKind::Strong ? "Strong" : "Weak",
      show(memory_location),
      points_tos);

  environment_.write(memory_location, field, points_tos, kind);
}

std::ostream& operator<<(
    std::ostream& out,
    const ForwardTaintEnvironment& environment) {
  return out << environment.environment_;
}

} // namespace marianatrench

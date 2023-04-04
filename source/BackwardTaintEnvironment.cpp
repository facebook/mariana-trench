/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/BackwardTaintEnvironment.h>

namespace marianatrench {

/* This must be called when accessing a specific path in backward taint. */
Taint BackwardTaintEnvironment::propagate_output_path(
    Taint taint,
    Path::Element path_element) {
  taint.append_to_propagation_output_paths(path_element);
  return taint;
}

BackwardTaintEnvironment BackwardTaintEnvironment::initial(
    MethodContext& context) {
  auto taint = TaintEnvironment::bottom();

  if (!context.method()->is_static()) {
    taint.set(
        context.memory_factory.make_parameter(0),
        TaintTree(Taint::propagation_taint(
            /* kind */ context.kind_factory.local_receiver(),
            /* output_paths */
            PathTreeDomain{{Path{}, SingletonAbstractDomain()}},
            /* inferred_features */ {},
            /* user_features */ {})));
  }

  return BackwardTaintEnvironment(std::move(taint));
}

TaintTree BackwardTaintEnvironment::read(
    MemoryLocation* memory_location) const {
  return taint_.get(memory_location->root())
      .read(memory_location->path(), propagate_output_path);
}

TaintTree BackwardTaintEnvironment::read(
    MemoryLocation* memory_location,
    const Path& path) const {
  Path full_path = memory_location->path();
  full_path.extend(path);

  return taint_.get(memory_location->root())
      .read(full_path, propagate_output_path);
}

TaintTree BackwardTaintEnvironment::read(
    const MemoryLocationsDomain& memory_locations) const {
  TaintTree taint;
  for (auto* memory_location : memory_locations.elements()) {
    taint.join_with(read(memory_location));
  }
  return taint;
}

void BackwardTaintEnvironment::write(
    MemoryLocation* memory_location,
    TaintTree taint,
    UpdateKind kind) {
  taint_.update(memory_location->root(), [&](const TaintTree& tree) {
    auto copy = tree;
    copy.write(memory_location->path(), std::move(taint), kind);
    return copy;
  });
}

void BackwardTaintEnvironment::write(
    MemoryLocation* memory_location,
    const Path& path,
    TaintTree taint,
    UpdateKind kind) {
  Path full_path = memory_location->path();
  full_path.extend(path);

  taint_.update(memory_location->root(), [&](const TaintTree& tree) {
    auto copy = tree;
    copy.write(full_path, std::move(taint), kind);
    return copy;
  });
}

void BackwardTaintEnvironment::write(
    MemoryLocation* memory_location,
    const Path& path,
    Taint taint,
    UpdateKind kind) {
  Path full_path = memory_location->path();
  full_path.extend(path);

  taint_.update(memory_location->root(), [&](const TaintTree& tree) {
    auto copy = tree;
    copy.write(full_path, std::move(taint), kind);
    return copy;
  });
}

void BackwardTaintEnvironment::write(
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

void BackwardTaintEnvironment::write(
    const MemoryLocationsDomain& memory_locations,
    Taint taint,
    UpdateKind kind) {
  write(memory_locations, TaintTree(std::move(taint)), kind);
}

void BackwardTaintEnvironment::write(
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

void BackwardTaintEnvironment::write(
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

std::ostream& operator<<(
    std::ostream& out,
    const BackwardTaintEnvironment& environment) {
  return out << environment.taint_;
}

} // namespace marianatrench

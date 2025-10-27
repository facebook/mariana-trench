/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/BackwardTaintEnvironment.h>
#include <mariana-trench/Heuristics.h>

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

  const Method* method = context.method();
  const bool is_static = method->is_static();
  if (!is_static) {
    taint.set(
        context.memory_factory.make_parameter(0),
        TaintTree(
            Taint::propagation_taint(
                /* kind */ context.kind_factory.local_receiver(),
                /* output_paths */
                PathTreeDomain{
                    {Path{},
                     CollapseDepth(
                         Heuristics::singleton()
                             .propagation_max_collapse_depth())}},
                /* inferred_features */ {},
                /* user_features */ {})));
  }

  if (context.options.propagate_across_arguments() &&
      !context.previous_model.is_frozen(Model::FreezeKind::Propagations)) {
    for (ParameterPosition i = method->first_parameter_index();
         i < method->number_of_parameters();
         ++i) {
      const DexType* argument_type = method->parameter_type(i);
      if (argument_type == nullptr || !type::is_object(argument_type)) {
        continue;
      }

      taint.set(
          context.memory_factory.make_parameter(i),
          TaintTree(
              Taint::propagation_taint(
                  /* kind */ context.kind_factory.local_argument(i),
                  /* output_paths */
                  PathTreeDomain{
                      {Path{},
                       CollapseDepth(
                           Heuristics::singleton()
                               .propagation_max_collapse_depth())}},
                  /* inferred_features */ {},
                  /* user_features */ {})));
    }
  }

  return BackwardTaintEnvironment(std::move(taint));
}

TaintTree BackwardTaintEnvironment::read(
    MemoryLocation* memory_location) const {
  return environment_.get(memory_location->root())
      .read(memory_location->path(), propagate_output_path);
}

TaintTree BackwardTaintEnvironment::read(
    MemoryLocation* memory_location,
    const Path& path) const {
  Path full_path = memory_location->path();
  full_path.extend(path);

  return environment_.get(memory_location->root())
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
  environment_.write(memory_location, Path{}, std::move(taint), kind);
}

void BackwardTaintEnvironment::write(
    MemoryLocation* memory_location,
    const Path& path,
    TaintTree taint,
    UpdateKind kind) {
  environment_.write(memory_location, path, std::move(taint), kind);
}

void BackwardTaintEnvironment::write(
    MemoryLocation* memory_location,
    const Path& path,
    Taint taint,
    UpdateKind kind) {
  write(memory_location, path, TaintTree{std::move(taint)}, kind);
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
  return out << environment.environment_;
}

} // namespace marianatrench

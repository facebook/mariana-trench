/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Scheduler.h>

namespace marianatrench {

// We use the dependency graph as the source of truth since it is more precise
// than the call graph (for instance, it takes into account
// `no-join-virtual-overrides`).
Scheduler::Scheduler(const Methods& methods, const Dependencies& dependencies)
    : strongly_connected_components_(methods, dependencies) {}

void Scheduler::schedule(
    const ConcurrentSet<const Method*>& methods,
    std::function<void(const Method*, std::size_t)> enqueue,
    unsigned int threads) const {
  // Schedule components by their reverse topological order (leaves to roots) in
  // the set of strongly connected components.
  std::size_t current_thread = 0;
  for (const auto& component : strongly_connected_components_.components()) {
    bool next_thread = false;
    // Schedule all methods in this component on the same thread.
    // Iterating on the reverse order here seems to give callees before callers
    // more often, even though this is not guaranteed by Tarjan's algorithm.
    for (auto iterator = component.rbegin(), end = component.rend();
         iterator != end;
         ++iterator) {
      const auto* method = *iterator;
      if (methods.count_unsafe(method) > 0) {
        next_thread = true;
        enqueue(method, current_thread);
      }
    }
    if (next_thread) {
      current_thread = (current_thread + 1) % threads;
    }
  }
}

} // namespace marianatrench

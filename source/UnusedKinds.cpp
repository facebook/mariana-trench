/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <SpartaWorkQueue.h>

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/UnusedKinds.h>

namespace marianatrench {

std::unordered_set<const Kind*> UnusedKinds::remove_unused_kinds(
    const Rules& rules,
    const Kinds& kinds,
    const Methods& methods,
    ArtificialMethods& artificial_methods,
    Registry& registry) {
  auto unused_kinds = rules.collect_unused_kinds(kinds);
  auto queue = sparta::work_queue<const Method*>(
      [&](const Method* method) {
        auto model = registry.get(method);
        model.remove_kinds(unused_kinds);
        registry.set(model);
      },
      sparta::parallel::default_num_threads());
  for (const auto* method : methods) {
    queue.add_item(method);
  }
  queue.run_all();
  artificial_methods.set_unused_kinds(unused_kinds);
  return unused_kinds;
}
} // namespace marianatrench

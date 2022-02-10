/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <SpartaWorkQueue.h>

#include <mariana-trench/Dependencies.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/PostprocessTraces.h>
#include <mariana-trench/Registry.h>

namespace marianatrench {

namespace {

bool is_valid_generation(
    const Method* MT_NULLABLE callee,
    const AccessPath& callee_port,
    const Kind* kind,
    const Registry& registry) {
  if (callee == nullptr) {
    // Leaf frame
    return true;
  } else if (callee_port.root().is_anchor()) {
    // Crtex frames which have canonical names instantiated during
    // `Frame::propagate` will have callee_ports `Anchor`. These are considered
    // leaves when it comes to traces (no next hop), even though the `Frame`
    // itself is not a leaf (has a callee).
    return true;
  }
  auto model = registry.get(callee);
  auto sources = model.generations().raw_read(callee_port).root();
  return sources.contains_kind(kind);
}

bool is_valid_sink(
    const Method* MT_NULLABLE callee,
    const AccessPath& callee_port,
    const Kind* kind,
    const Registry& registry) {
  if (callee == nullptr) {
    // Leaf frame
    return true;
  } else if (callee_port.root().is_anchor()) {
    // Crtex frames which have canonical names instantiated during
    // `Frame::propagate` will have callee_ports `Anchor`. These are considered
    // leaves when it comes to traces (no next hop), even though the `Frame`
    // itself is not a leaf (has a callee).
    return true;
  }

  auto model = registry.get(callee);
  auto sinks = model.sinks().raw_read(callee_port).root();

  if (sinks.contains_kind(kind)) {
    return true;
  }

  // For triggered kinds, this is trickier. Its callee's kind (frame_kind)
  // could be a PartialKind that turned into a TriggeredPartialKind in the
  // caller (sink_kind). The sink is valid as long as its underlying partial
  // kind matches that of its callee's.
  const auto* sink_triggered_kind = kind->as<TriggeredPartialKind>();
  if (!sink_triggered_kind) {
    return false;
  }

  return sinks.contains_kind(sink_triggered_kind->partial_kind());
}

TaintAccessPathTree cull_collapsed_generations(
    TaintAccessPathTree generation_tree,
    const Registry& registry) {
  generation_tree.map([&](Taint& generation_taint) {
    generation_taint.filter_invalid_frames([&](const Method* MT_NULLABLE callee,
                                               const AccessPath& callee_port,
                                               const Kind* kind) {
      return is_valid_generation(callee, callee_port, kind, registry);
    });
  });
  return generation_tree;
}

TaintAccessPathTree cull_collapsed_sinks(
    TaintAccessPathTree sink_tree,
    const Registry& registry) {
  sink_tree.map([&](Taint& sink_taint) {
    sink_taint.filter_invalid_frames([&](const Method* MT_NULLABLE callee,
                                         const AccessPath& callee_port,
                                         const Kind* kind) {
      return is_valid_sink(callee, callee_port, kind, registry);
    });
  });
  return sink_tree;
}

IssueSet cull_collapsed_issues(IssueSet issues, const Registry& registry) {
  issues.map([&](Issue& issue) {
    issue.filter_sources([&](const Method* MT_NULLABLE callee,
                             const AccessPath& callee_port,
                             const Kind* kind) {
      return is_valid_generation(callee, callee_port, kind, registry);
    });
    issue.filter_sinks([&](const Method* MT_NULLABLE callee,
                           const AccessPath& callee_port,
                           const Kind* kind) {
      return is_valid_sink(callee, callee_port, kind, registry);
    });
  });
  return issues;
}

} // namespace

void PostprocessTraces::remove_collapsed_traces(
    Registry& registry,
    const Context& context) {
  // We need to compute a decreasing fixpoint since we might remove empty
  // generations or sinks that are referenced in other models.

  auto methods = std::make_unique<ConcurrentSet<const Method*>>();
  for (const auto* method : *context.methods) {
    methods->insert(method);
  }

  while (methods->size() > 0) {
    auto new_methods = std::make_unique<ConcurrentSet<const Method*>>();

    auto queue = sparta::work_queue<const Method*>(
        [&](const Method* method) {
          const auto old_model = registry.get(method);

          auto model = old_model;
          model.set_generations(
              cull_collapsed_generations(model.generations(), registry));
          model.set_sinks(cull_collapsed_sinks(model.sinks(), registry));
          model.set_issues(cull_collapsed_issues(model.issues(), registry));

          if (!old_model.leq(model)) {
            for (const auto* dependency :
                 context.dependencies->dependencies(method)) {
              new_methods->insert(dependency);
            }
          }

          registry.set(model);
        },
        sparta::parallel::default_num_threads());
    for (const auto* method : *methods) {
      queue.add_item(method);
    }
    queue.run_all();
    methods = std::move(new_methods);
  }
}

} // namespace marianatrench

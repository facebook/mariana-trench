/*
 * Copyright (c) Facebook, Inc. and its affiliates.
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

bool is_valid_generation(const Frame& generation, const Registry& registry) {
  if (generation.is_leaf()) {
    return true;
  }
  auto model = registry.get(generation.callee());
  auto sources = model.generations().raw_read(generation.callee_port()).root();
  return std::any_of(
      sources.begin(), sources.end(), [&](const FrameSet& frames) {
        return frames.kind() == generation.kind();
      });
}

bool is_valid_sink(const Frame& sink, const Registry& registry) {
  if (sink.is_leaf()) {
    return true;
  }
  auto model = registry.get(sink.callee());
  auto sinks = model.sinks().raw_read(sink.callee_port()).root();
  return std::any_of(sinks.begin(), sinks.end(), [&](const FrameSet& frames) {
    return frames.kind() == sink.kind();
  });
}

TaintAccessPathTree cull_collapsed_generations(
    TaintAccessPathTree generation_tree,
    const Registry& registry) {
  generation_tree.map([&](Taint& generation_taint) {
    generation_taint.map([&](FrameSet& generations) {
      generations.filter([&](const Frame& generation) {
        return is_valid_generation(generation, registry);
      });
    });
  });
  return generation_tree;
}

TaintAccessPathTree cull_collapsed_sinks(
    TaintAccessPathTree sink_tree,
    const Registry& registry) {
  sink_tree.map([&](Taint& sink_taint) {
    sink_taint.map([&](FrameSet& sinks) {
      sinks.filter(
          [&](const Frame& sink) { return is_valid_sink(sink, registry); });
    });
  });
  return sink_tree;
}

IssueSet cull_collapsed_issues(IssueSet issues, const Registry& registry) {
  issues.map([&](Issue& issue) {
    issue.filter_sources([&](const Frame& generation) {
      return is_valid_generation(generation, registry);
    });
    issue.filter_sinks(
        [&](const Frame& sink) { return is_valid_sink(sink, registry); });
  });
  return issues;
}

} // namespace

void PostprocessTraces::postprocess_remove_collapsed_traces(
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

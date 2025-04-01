/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <sparta/WorkQueue.h>

#include <mariana-trench/Dependencies.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/PostprocessTraces.h>
#include <mariana-trench/Registry.h>
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {

namespace {

bool check_callee_kinds(
    const Context& context,
    const Kind* kind,
    const Taint& callee_taint) {
  const auto* transform_kind = kind->as<TransformKind>();
  if (transform_kind == nullptr) {
    return callee_taint.contains_kind(kind);
  }

  const auto* base_kind = transform_kind->base_kind();
  const auto* global_transforms = transform_kind->global_transforms();
  if (global_transforms == nullptr) {
    // transform_kind only has local transforms, in which case we can just check
    // for the base_kind in the callee.
    return callee_taint.contains_kind(base_kind);
  }

  // Using an exception to break out of the loop early since `visit_frames`
  // does not allow us to do that.
  class frame_found : public std::exception {};
  try {
    callee_taint.visit_frames([base_kind, global_transforms, &context](
                                  const CallInfo&, const Frame& frame) {
      const auto* frame_kind = frame.kind();
      if (frame_kind == nullptr) {
        return;
      }

      const auto* frame_transform_kind = frame_kind->as<TransformKind>();
      if (frame_transform_kind == nullptr ||
          frame_transform_kind->base_kind() != base_kind) {
        // No match
        return;
      }

      if (global_transforms ==
          context.transforms_factory->concat(
              frame_transform_kind->local_transforms(),
              frame_transform_kind->global_transforms())) {
        throw frame_found();
      }
    });
  } catch (const frame_found&) {
    return true;
  }

  return false;
}

bool is_valid_generation(
    const Context& context,
    const Method* MT_NULLABLE callee,
    const AccessPath* MT_NULLABLE callee_port,
    const Kind* kind,
    const Registry& registry) {
  if (callee == nullptr) {
    // Leaf frame
    return true;
  }

  // Port should not be null for non-leaf frames (i.e. when callee != nullptr)
  mt_assert(callee_port != nullptr);

  if (callee_port->root().is_anchor()) {
    // Crtex frames which have canonical names instantiated during
    // `Frame::propagate` will have callee_ports `Anchor`. These are considered
    // leaves when it comes to traces (no next hop), even though the `Frame`
    // itself is not a leaf (has a callee).
    return true;
  }
  auto model = registry.get(callee);

  return check_callee_kinds(
      context, kind, model.generations().raw_read(*callee_port).root());
}

bool is_valid_sink(
    const Context& context,
    const Method* MT_NULLABLE callee,
    const AccessPath* MT_NULLABLE callee_port,
    const Kind* kind,
    const Registry& registry) {
  if (callee == nullptr) {
    // Leaf frame
    return true;
  }

  // Port should not be null for non-leaf frames (i.e. when callee != nullptr)
  mt_assert(callee_port != nullptr);

  if (callee_port->root().is_anchor()) {
    // Crtex frames which have canonical names instantiated during
    // `Frame::propagate` will have callee_ports `Anchor`. These are considered
    // leaves when it comes to traces (no next hop), even though the `Frame`
    // itself is not a leaf (has a callee).
    return true;
  } else if (callee_port->root().is_call_effect()) {
    return true;
  }

  auto model = registry.get(callee);
  auto sinks = model.sinks().raw_read(*callee_port).root();

  if (check_callee_kinds(context, kind, sinks)) {
    return true;
  }

  // For triggered kinds, this is trickier. Its callee's kind (frame_kind)
  // could be a PartialKind that turned into a TriggeredPartialKind in the
  // caller (sink_kind). The sink is valid as long as its underlying partial
  // kind matches that of its callee's.
  // Transforms are not supported for PartialKinds
  const auto* sink_triggered_kind =
      kind->discard_transforms()->as<TriggeredPartialKind>();
  if (!sink_triggered_kind) {
    return false;
  }

  return sinks.contains_kind(sink_triggered_kind->partial_kind());
}

TaintAccessPathTree cull_collapsed_generations(
    const Context& context,
    TaintAccessPathTree generation_tree,
    const Registry& registry) {
  generation_tree.transform([&context, &registry](Taint generation_taint) {
    generation_taint.filter_invalid_frames(
        [&context, &registry](
            const Method* MT_NULLABLE callee,
            const AccessPath* MT_NULLABLE callee_port,
            const Kind* kind) {
          return is_valid_generation(
              context, callee, callee_port, kind, registry);
        });
    return generation_taint;
  });
  return generation_tree;
}

TaintAccessPathTree cull_collapsed_sinks(
    const Context& context,
    TaintAccessPathTree sink_tree,
    const Registry& registry) {
  sink_tree.transform([&context, &registry](Taint sink_taint) {
    sink_taint.filter_invalid_frames(
        [&context, &registry](
            const Method* MT_NULLABLE callee,
            const AccessPath* MT_NULLABLE callee_port,
            const Kind* kind) {
          return is_valid_sink(context, callee, callee_port, kind, registry);
        });
    return sink_taint;
  });
  return sink_tree;
}

IssueSet cull_collapsed_issues(
    const Context& context,
    IssueSet issues,
    const Registry& registry) {
  issues.transform([&context, &registry](Issue issue) {
    issue.filter_sources([&context, &registry](
                             const Method* MT_NULLABLE callee,
                             const AccessPath* MT_NULLABLE callee_port,
                             const Kind* kind) {
      return is_valid_generation(context, callee, callee_port, kind, registry);
    });
    issue.filter_sinks([&context, &registry](
                           const Method* MT_NULLABLE callee,
                           const AccessPath* MT_NULLABLE callee_port,
                           const Kind* kind) {
      return is_valid_sink(context, callee, callee_port, kind, registry);
    });
    return issue;
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
          model.set_generations(cull_collapsed_generations(
              context, model.generations(), registry));
          model.set_sinks(
              cull_collapsed_sinks(context, model.sinks(), registry));
          model.set_issues(
              cull_collapsed_issues(context, model.issues(), registry));

          if (!old_model.leq(model)) {
            for (const auto* dependency :
                 context.dependencies->dependencies(method)) {
              new_methods->insert(dependency);
            }
          }

          registry.set(model);
        },
        sparta::parallel::default_num_threads());
    for (const auto* method : UnorderedIterable(*methods)) {
      queue.add_item(method);
    }
    queue.run_all();
    methods = std::move(new_methods);
  }
}

} // namespace marianatrench

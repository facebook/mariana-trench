/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Assert.h>
#include <mariana-trench/Kinds.h>
#include <mariana-trench/TransformOperations.h>
#include <mariana-trench/Transforms.h>

namespace marianatrench {
namespace transforms {

PropagationInfo apply_propagation(
    MethodContext* context,
    const Frame& propagation,
    const TaintTree& input_taint_tree) {
  const auto* kind = propagation.kind();
  mt_assert(kind != nullptr);

  if (const auto* propagation_kind = kind->as<PropagationKind>()) {
    return PropagationInfo{propagation_kind, input_taint_tree};
  }

  const auto* transform_kind = kind->as<TransformKind>();
  mt_assert(transform_kind != nullptr);
  mt_assert(transform_kind->global_transforms() == nullptr);

  const auto* propagation_kind =
      transform_kind->base_kind()->as<PropagationKind>();
  mt_assert(propagation_kind != nullptr);

  TaintTree output_taint_tree{};
  for (const auto& [path, taint] : input_taint_tree.elements()) {
    output_taint_tree.write(
        path,
        taint.apply_transform(
            context->kinds,
            context->transforms,
            context->used_kinds,
            transform_kind->local_transforms()),
        UpdateKind::Weak);
  }

  return PropagationInfo{propagation_kind, std::move(output_taint_tree)};
}

const Kind* MT_NULLABLE get_propagation_for_artificial_source(
    MethodContext* context,
    const PropagationKind* propagation_kind,
    const Kind* artificial_source) {
  if (artificial_source == Kinds::artificial_source()) {
    return propagation_kind;
  }

  const auto* transform_kind = artificial_source->as<TransformKind>();
  mt_assert(transform_kind != nullptr);
  mt_assert(transform_kind->base_kind() == Kinds::artificial_source());

  return context->kinds.transform_kind(
      propagation_kind,
      context->transforms.reverse(transform_kind->local_transforms()),
      context->transforms.reverse(transform_kind->global_transforms()));
}

namespace {

FeatureMayAlwaysSet get_fulfilled_sink_features(
    const FulfilledPartialKindState& fulfilled_partial_sinks,
    const Kind* transformed_sink_kind) {
  const auto* new_kind = transformed_sink_kind->as<TriggeredPartialKind>();
  // Called only after transform_kind_with_features creates a triggered kind,
  // so this must be a TriggeredPartialKind.
  mt_assert(new_kind != nullptr);
  const auto* rule = new_kind->rule();
  const auto* counterpart = fulfilled_partial_sinks.get_fulfilled_counterpart(
      /* unfulfilled_kind */ new_kind->partial_kind(), rule);

  // A triggered kind was created, so its counterpart must exist.
  mt_assert(counterpart != nullptr);
  return fulfilled_partial_sinks.get_features(counterpart, rule);
}

} // namespace

Taint get_sink_for_artificial_source(
    MethodContext* context,
    Taint new_sinks,
    const Kind* artificial_source,
    const FulfilledPartialKindState& fulfilled_partial_sinks) {
  // Handle partial kinds.
  new_sinks.transform_kind_with_features(
      [context, &fulfilled_partial_sinks](
          const Kind* sink_kind) -> std::vector<const Kind*> {
        const Kind* base_kind = sink_kind;
        const auto* transform_kind = sink_kind->as<TransformKind>();

        if (transform_kind != nullptr) {
          base_kind = transform_kind->base_kind();
        }

        const auto* partial_sink = base_kind->as<PartialKind>();
        if (!partial_sink) {
          // Not a PartialKind. Keep sink as it is.
          return {sink_kind};
        }

        // Transforms are not supported for multi source/sink rules. Simply
        // return the fulfilled counterparts.
        return fulfilled_partial_sinks.make_triggered_counterparts(
            /* unfulfilled_kind */ partial_sink, context->kinds);
      },
      [&fulfilled_partial_sinks](const Kind* new_kind) {
        return get_fulfilled_sink_features(fulfilled_partial_sinks, new_kind);
      });

  if (artificial_source == Kinds::artificial_source()) {
    // No transforms to apply. Return a copy of sinks.
    return new_sinks;
  }

  const auto* transform_kind = artificial_source->as<TransformKind>();
  mt_assert(transform_kind != nullptr);
  mt_assert(transform_kind->base_kind() == Kinds::artificial_source());
  mt_assert(transform_kind->global_transforms() == nullptr);

  return new_sinks.apply_transform(
      context->kinds,
      context->transforms,
      context->used_kinds,
      context->transforms.reverse(transform_kind->local_transforms()));
}

} // namespace transforms
} // namespace marianatrench

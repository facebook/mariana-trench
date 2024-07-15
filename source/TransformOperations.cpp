/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Assert.h>
#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/TransformOperations.h>
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {
namespace transforms {

TaintTree apply_propagation(
    MethodContext* context,
    const CallInfo& propagation_call_info,
    const Frame& propagation_frame,
    TaintTree input_taint_tree,
    TransformDirection direction) {
  const auto* kind = propagation_frame.kind();
  mt_assert(kind != nullptr);

  if (kind->is<PropagationKind>()) {
    return input_taint_tree;
  }

  const auto* transform_kind = kind->as<TransformKind>();
  mt_assert(transform_kind != nullptr);
  // For propagations with traces, we can have local and global transforms the
  // same as with source/sink traces. Regardless, both local and global
  // transforms in the propagation are local to the call-site where it's
  // applied. Hence we combine them here before applying it.
  const auto* all_transforms = context->transforms_factory.concat(
      transform_kind->local_transforms(), transform_kind->global_transforms());
  mt_assert(all_transforms != nullptr);

  mt_assert(
      propagation_call_info.call_kind().is_propagation_with_trace() ||
      propagation_call_info.call_kind().is_propagation() &&
          std::any_of(
              all_transforms->begin(),
              all_transforms->end(),
              [](const Transform* transform) {
                return transform->is<SanitizerSetTransform>();
              }));

  if (direction == TransformDirection::Forward) {
    all_transforms = context->transforms_factory.reverse(all_transforms);
  }

  const auto* propagation_kind =
      transform_kind->base_kind()->as<PropagationKind>();
  mt_assert(propagation_kind != nullptr);

  TaintTree output_taint_tree{};
  for (const auto& [path, taint] : input_taint_tree.elements()) {
    output_taint_tree.write(
        path,
        taint.apply_transform(
            context->kind_factory,
            context->transforms_factory,
            context->used_kinds,
            all_transforms),
        UpdateKind::Weak);
  }

  // For propagation with traces, we need to update the output taint tree with
  // the trace information from the propagation frame.
  output_taint_tree.update_with_propagation_trace(
      propagation_call_info, propagation_frame);

  return output_taint_tree;
}

Taint apply_source_as_transform_to_sink(
    MethodContext* context,
    const Taint& source_taint,
    const TransformList* source_as_transform,
    const Taint& sink_taint,
    std::string_view callee) {
  auto transformed_sink_taint = sink_taint.apply_transform(
      context->kind_factory,
      context->transforms_factory,
      context->used_kinds,
      source_as_transform);

  // Add additional features
  transformed_sink_taint.add_locally_inferred_features(
      FeatureMayAlwaysSet{context->feature_factory.get_exploitability_root()});

  // Add extra trace and exploitability origin to source frame
  transformed_sink_taint.update_with_extra_trace_and_exploitability_origin(
      source_taint, FrameType::source(), context->method(), callee);

  LOG(5, "Materialized source-as-transform sink: {}", transformed_sink_taint);

  return transformed_sink_taint;
}

} // namespace transforms
} // namespace marianatrench

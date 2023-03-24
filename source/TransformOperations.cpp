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

} // namespace transforms
} // namespace marianatrench

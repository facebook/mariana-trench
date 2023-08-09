/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Assert.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/TransformOperations.h>
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {
namespace transforms {

TaintTree apply_propagation(
    MethodContext* context,
    const Frame& propagation,
    TaintTree input_taint_tree) {
  const auto* kind = propagation.kind();
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

  return output_taint_tree;
}

} // namespace transforms
} // namespace marianatrench

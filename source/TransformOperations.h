/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Frame.h>
#include <mariana-trench/MethodContext.h>
#include <mariana-trench/TaintTree.h>

namespace marianatrench {

struct PropagationInfo {
  const PropagationKind* propagation_kind;
  TaintTree output_taint_tree;
};

namespace transforms {

PropagationInfo apply_propagation(
    MethodContext* context,
    const Frame& propagation,
    const TaintTree& input_taint_tree);

const Kind* MT_NULLABLE get_propagation_for_artificial_source(
    MethodContext* context,
    const PropagationKind* propagation_kind,
    const Kind* artificial_source);

Taint get_sink_for_artificial_source(
    MethodContext* context,
    Taint sinks,
    const Kind* artificial_source,
    const FulfilledPartialKindState& fulfilled_partial_sinks);

} // namespace transforms
} // namespace marianatrench

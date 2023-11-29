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

namespace transforms {

TaintTree apply_propagation(
    MethodContext* context,
    const CallInfo& propagation_call_info,
    const Frame& propagation_frame,
    TaintTree input_taint_tree);

} // namespace transforms
} // namespace marianatrench

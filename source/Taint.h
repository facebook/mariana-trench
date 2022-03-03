/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/TaintV1.h>
#include <mariana-trench/TaintV2.h>

namespace marianatrench {

using Taint = TaintV1;

// Use this to switch to TaintV2's implementation when it is ready.
// using Taint = TaintV2;

} // namespace marianatrench

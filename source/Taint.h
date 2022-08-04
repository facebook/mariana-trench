/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/TaintV2.h>

namespace marianatrench {

// TODO(T91357916): Rename TaintV2 to Taint. V1 no longer supported.
using Taint = TaintV2;

} // namespace marianatrench

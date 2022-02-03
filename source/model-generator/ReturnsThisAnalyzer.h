/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Method.h>

namespace marianatrench {
namespace returns_this_analyzer {

bool method_returns_this(const Method* method);

} // namespace returns_this_analyzer
} // namespace marianatrench

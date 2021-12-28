/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Context.h>
#include <mariana-trench/Registry.h>

namespace marianatrench {

class Interprocedural final {
 public:
  static void run_analysis(Context& context, Registry& registry);
};

} // namespace marianatrench

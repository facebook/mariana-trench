/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Context.h>
#include <mariana-trench/MethodMappings.h>
#include <mariana-trench/shim-generator/Shims.h>

namespace marianatrench {

class ShimGeneratorError : public std::invalid_argument {
 public:
  explicit ShimGeneratorError(const std::string& message);
};

class ShimGeneration {
 public:
  static Shims run(
      Context& context,
      const IntentRoutingAnalyzer& intent_routing_analyzer,
      const MethodMappings& method_mappings);
};

} // namespace marianatrench

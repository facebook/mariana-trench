/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <functional>

#include <ConcurrentContainers.h>

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/IntentRoutingAnalyzer.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/shim-generator/Shim.h>

namespace marianatrench {

class Shims final {
 private:
  using MethodToShimMap = std::unordered_map<const Method*, InstantiatedShim>;

 public:
  explicit Shims(
      std::size_t global_shims_size,
      const IntentRoutingAnalyzer& intent_routing_analyzer)
      : global_shims_(global_shims_size),
        intent_routing_analyzer_(std::cref(intent_routing_analyzer)) {}

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Shims)

  std::optional<Shim> get_shim_for_caller(
      const Method* original_callee,
      const Method* caller) const;

  bool add_instantiated_shim(const InstantiatedShim& shim);

 private:
  MethodToShimMap global_shims_;

  std::reference_wrapper<const IntentRoutingAnalyzer> intent_routing_analyzer_;
};

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <unordered_map>

#include <boost/algorithm/string/predicate.hpp>

#include <mariana-trench/Constants.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/shim-generator/Shims.h>

namespace marianatrench {

namespace {
bool skip_shim_for_caller(const Method* caller) {
  static const std::vector<std::string> k_exclude_caller_in_packages = {
      "Landroid/",
      "Landroidx/",
      "Lcom/google/",
  };

  const auto caller_klass = caller->get_class()->str();
  return std::any_of(
      k_exclude_caller_in_packages.begin(),
      k_exclude_caller_in_packages.end(),
      [caller_klass](const auto& exclude_prefix) {
        return boost::starts_with(caller_klass, exclude_prefix);
      });
}

} // namespace

bool Shims::add_instantiated_shim(const InstantiatedShim& shim) {
  return global_shims_.emplace(shim.method(), shim).second;
}

std::optional<Shim> Shims::get_shim_for_caller(
    const Method* original_callee,
    const Method* caller) const {
  auto shim = global_shims_.find(original_callee);
  if (skip_shim_for_caller(caller)) {
    return std::nullopt;
  }
  const InstantiatedShim* MT_NULLABLE instantiated_shim = nullptr;
  if (shim != global_shims_.end()) {
    instantiated_shim = &(shim->second);
  }

  std::vector<ShimTarget> intent_routing_targets =
      intent_routing_analyzer_.get_intent_routing_targets(
          original_callee, caller);

  if (instantiated_shim == nullptr && intent_routing_targets.empty()) {
    return std::nullopt;
  }
  return Shim{instantiated_shim, intent_routing_targets};
}

} // namespace marianatrench

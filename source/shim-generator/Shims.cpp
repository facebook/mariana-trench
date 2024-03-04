/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <unordered_map>
#include <unordered_set>

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

void Shims::add_instantiated_shim(InstantiatedShim shim) {
  const auto* method = shim.method();
  auto it = global_shims_.find(method);
  if (it != global_shims_.end()) {
    it->second.merge_with(std::move(shim));
  } else {
    global_shims_.emplace(method, std::move(shim));
  }
}

void Shims::add_intent_routing_analyzer(
    std::unique_ptr<IntentRoutingAnalyzer> intent_routing_analyzer) {
  intent_routing_analyzer_ = std::move(intent_routing_analyzer);
}

std::optional<Shim> Shims::get_shim_for_caller(
    const Method* original_callee,
    const Method* caller) const {
  auto shim = global_shims_.find(original_callee);
  if (skip_shim_for_caller(caller)) {
    return std::nullopt;
  }
  const InstantiatedShim* instantiated_shim = nullptr;
  if (shim != global_shims_.end()) {
    instantiated_shim = &(shim->second);
  }

  auto intent_routing_targets = intent_routing_analyzer_
      ? intent_routing_analyzer_->get_intent_routing_targets(
            original_callee, caller)
      : std::unordered_set<ShimTarget>{};

  if (instantiated_shim == nullptr && intent_routing_targets.empty()) {
    return std::nullopt;
  }

  return Shim{instantiated_shim, intent_routing_targets};
}

} // namespace marianatrench

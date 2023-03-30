/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string/predicate.hpp>

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

bool Shims::add_global_method_shim(const Shim& shim) {
  return global_shims_.emplace(shim.method(), shim).second;
}

std::optional<Shim> Shims::get_shim_for_caller(
    const Method* original_callee,
    const Method* caller) const {
  auto shim = global_shims_.find(original_callee);
  if (shim == global_shims_.end() || skip_shim_for_caller(caller)) {
    return std::nullopt;
  }
  return shim->second;
}

void Shims::add_intent_routing_data(
    const Method* method,
    IntentRoutingData&& intent_routing_data) {
  if (intent_routing_data.calls_get_intent) {
    LOG(5, "Shimming {} as a method that calls getIntent().", method->show());
    auto klass = method->get_class();
    classes_to_intent_getters_.update(
        klass,
        [&method](
            const DexType* /* key */,
            std::vector<const Method*>& methods,
            bool /* exists */) {
          methods.emplace_back(method);
          return methods;
        });
  }

  if (!intent_routing_data.routed_intents.empty()) {
    LOG(5,
        "Shimming {} as a method that routes intents cross-component.",
        method->show());
    methods_to_routed_intents_.emplace(
        method, intent_routing_data.routed_intents);
  }
}

const MethodToRoutedIntentClassesMap& Shims::methods_to_routed_intents() const {
  return methods_to_routed_intents_;
}

const ClassesToIntentGettersMap& Shims::classes_to_intent_getters() const {
  return classes_to_intent_getters_;
}

} // namespace marianatrench

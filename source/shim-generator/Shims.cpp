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

bool is_activity_routing_method(const Method* method) {
  // TODO(T149770577): This should be configurable with model generator syntax.
  LOG(4, "checking if {} activity", method->get_name());
  return method->get_name() == "startActivity";
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
  std::vector<ShimTarget> intent_routing_targets;
  // Intent routing does not exist unless the callee is in the vein of
  // startActivity(intent).
  if (is_activity_routing_method(original_callee)) {
    auto routed_intent_classes = methods_to_routed_intents_.find(caller);
    if (routed_intent_classes != methods_to_routed_intents_.end()) {
      for (const auto& intent_class : routed_intent_classes->second) {
        if (auto intent_getters = classes_to_intent_getters_.find(intent_class);
            intent_getters != classes_to_intent_getters_.end()) {
          for (const auto& intent_getter : intent_getters->second) {
            // TODO(T149770577): Similar to is_activity_routing method, use a
            // DSL to allow users to specify which argument maps to the intent,
            // or infer it by type. For now, hard-code to argument(1) because
            // that is what startActivity(intent) uses for the intent.
            intent_routing_targets.emplace_back(
                intent_getter,
                ShimParameterMapping{},
                /* call_effect_parameter_mapping */
                std::unordered_map<Root, ParameterPosition>{
                    {Root(Root::Kind::CallEffectIntent), 1}});
          }
        }
      }
    }
  }
  if (instantiated_shim == nullptr && intent_routing_targets.empty()) {
    return std::nullopt;
  }
  return Shim{instantiated_shim, intent_routing_targets};
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

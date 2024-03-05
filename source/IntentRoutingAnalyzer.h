/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ConcurrentContainers.h>

#include <mariana-trench/Constants.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Types.h>
#include <mariana-trench/shim-generator/Shim.h>

namespace marianatrench {

struct ReceivingMethod {
  const Method* method;
  std::optional<Root> root;
  std::optional<Component> component;
};

class IntentRoutingAnalyzer final {
 public:
  using MethodToRoutedIntentClassesMap =
      ConcurrentMap<const Method*, std::vector<const DexType*>>;
  using ClassesToIntentReceiversMap =
      ConcurrentMap<const DexType*, std::vector<ReceivingMethod>>;

  explicit IntentRoutingAnalyzer() = default;

 public:
  static std::unique_ptr<IntentRoutingAnalyzer>
  run(const Methods& methods, const Types& types, const Options& options);

  // Move constructor required for static run() method to return the object.
  MOVE_CONSTRUCTOR_ONLY(IntentRoutingAnalyzer);

  InstantiatedShim::FlatSet<ShimTarget> get_intent_routing_targets(
      const Method* original_callee,
      const Method* caller) const;

  const MethodToRoutedIntentClassesMap& methods_to_routed_intents() const {
    return methods_to_routed_intents_;
  }

  const ClassesToIntentReceiversMap& classes_to_intent_receivers() const {
    return classes_to_intent_receivers_;
  }

 private:
  MethodToRoutedIntentClassesMap methods_to_routed_intents_;
  ClassesToIntentReceiversMap classes_to_intent_receivers_;
};

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ConcurrentContainers.h>

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Types.h>
#include <mariana-trench/shim-generator/Shim.h>

namespace marianatrench {

class IntentRoutingAnalyzer final {
 public:
  using MethodToRoutedIntentClassesMap =
      ConcurrentMap<const Method*, std::vector<const DexType*>>;
  using ClassesToIntentGettersMap =
      ConcurrentMap<const DexType*, std::vector<const Method*>>;

  explicit IntentRoutingAnalyzer() = default;

 public:
  static IntentRoutingAnalyzer run(const Context&);

  // Move constructor required for static run() method to return the object.
  MOVE_CONSTRUCTOR_ONLY(IntentRoutingAnalyzer);

  std::vector<ShimTarget> get_intent_routing_targets(
      const Method* original_callee,
      const Method* caller) const;

  const MethodToRoutedIntentClassesMap& methods_to_routed_intents() const {
    return methods_to_routed_intents_;
  }

  const ClassesToIntentGettersMap& classes_to_intent_getters() const {
    return classes_to_intent_getters_;
  }

 private:
  MethodToRoutedIntentClassesMap methods_to_routed_intents_;
  ClassesToIntentGettersMap classes_to_intent_getters_;
};

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ConcurrentContainers.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/IntentRoutingAnalyzer.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/shim-generator/Shim.h>

namespace marianatrench {

using MethodToRoutedIntentClassesMap =
    ConcurrentMap<const Method*, std::vector<const DexType*>>;
using ClassesToIntentGettersMap =
    ConcurrentMap<const DexType*, std::vector<const Method*>>;

class Shims final {
 private:
  using MethodToShimMap = std::unordered_map<const Method*, InstantiatedShim>;

 public:
  explicit Shims(std::size_t global_shims_size)
      : global_shims_(global_shims_size),
        methods_to_routed_intents_({}),
        classes_to_intent_getters_({}) {}

  explicit Shims() = default;

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Shims)

  std::optional<Shim> get_shim_for_caller(
      const Method* original_callee,
      const Method* caller) const;

  bool add_instantiated_shim(const InstantiatedShim& shim);

  void add_intent_routing_data(
      const Method* method,
      IntentRoutingData&& intent_routing_data);

  const MethodToRoutedIntentClassesMap& methods_to_routed_intents() const;
  const ClassesToIntentGettersMap& classes_to_intent_getters() const;

 private:
  MethodToShimMap global_shims_;
  MethodToRoutedIntentClassesMap methods_to_routed_intents_;
  ClassesToIntentGettersMap classes_to_intent_getters_;
};

} // namespace marianatrench

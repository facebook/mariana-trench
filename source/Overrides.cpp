// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <memory>
#include <vector>

#include <MethodOverrideGraph.h>
#include <Show.h>
#include <Walkers.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Overrides.h>

namespace marianatrench {

Overrides::Overrides(
    const Methods& method_factory,
    const DexStoresVector& stores) {
  // Compute overrides.
  std::vector<std::unique_ptr<const method_override_graph::Graph>>
      method_override_graphs;
  for (const auto& scope : DexStoreClassesIterator(stores)) {
    method_override_graphs.push_back(method_override_graph::build_graph(scope));
  }

  // Record overrides.
  for (const auto& scope : DexStoreClassesIterator(stores)) {
    walk::parallel::methods(scope, [&](const DexMethod* dex_method) {
      std::unordered_set<const Method*> method_overrides;
      for (const auto& graph : method_override_graphs) {
        auto overrides_in_scope = method_override_graph::get_overriding_methods(
            *graph, dex_method, /* include_interfaces */ true);
        for (const auto* override : overrides_in_scope) {
          method_overrides.insert(method_factory.get(override));
        }
      }

      set(method_factory.get(dex_method), std::move(method_overrides));
    });
  }
}

const std::unordered_set<const Method*>& Overrides::get(
    const Method* method) const {
  auto* overrides = overrides_.get(method, /* default */ nullptr);
  if (overrides != nullptr) {
    return *overrides;
  } else {
    return empty_method_set_;
  }
}

void Overrides::set(
    const Method* method,
    std::unordered_set<const Method*> overrides) {
  if (overrides.empty()) {
    mt_assert(overrides_.get(method, /* default */ nullptr) == nullptr);
    return;
  }

  overrides_.emplace(
      method,
      std::make_unique<std::unordered_set<const Method*>>(
          std::move(overrides)));
}

const std::unordered_set<const Method*>& Overrides::empty_method_set() const {
  return empty_method_set_;
}

Json::Value Overrides::to_json() const {
  auto value = Json::Value(Json::objectValue);
  for (auto [method, overrides] : overrides_) {
    auto overrides_value = Json::Value(Json::arrayValue);
    for (const auto* override : *overrides) {
      overrides_value.append(Json::Value(show(override)));
    }
    value[show(method)] = overrides_value;
  }
  return value;
}

} // namespace marianatrench

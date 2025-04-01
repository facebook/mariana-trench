/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_set>

#include <json/json.h>

#include <DexStore.h>

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/UniquePointerConcurrentMap.h>

namespace marianatrench {

class Overrides final {
 public:
  using MapType =
      std::unordered_map<const Method*, std::unordered_set<const Method*>>;

 public:
  explicit Overrides(
      const Options& options,
      AnalysisMode analysis_mode,
      Methods& methods,
      const DexStoresVector& stores);

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Overrides)

  /**
   * Return the set of methods overriding the given method.
   */
  const std::unordered_set<const Method*>& get(const Method* method) const;

  /**
   * Set the override set of the given method.
   *
   * This is thread-safe if no other thread holds a reference on the override
   * set of the given method.
   */
  void set(const Method* method, std::unordered_set<const Method*> overrides);

  const std::unordered_set<const Method*>& empty_method_set() const;

  bool has_obscure_override_for(const Method* method) const;

  Json::Value to_json() const;

  static MapType from_json(const Json::Value& value, Methods& methods);

 private:
  // To be called from the constructor based on AnalysisMode.
  void add_cached_overrides(const Options& options, Methods& methods);
  void init_from_stores(const DexStoresVector& stores, Methods& method_factory);

 private:
  UniquePointerConcurrentMap<const Method*, std::unordered_set<const Method*>>
      overrides_;
  std::unordered_set<const Method*> empty_method_set_;
};

} // namespace marianatrench

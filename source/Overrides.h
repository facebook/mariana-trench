/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_set>

#include <json/json.h>

#include <DexStore.h>

#include <mariana-trench/Method.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/UniquePointerConcurrentMap.h>

namespace marianatrench {

class Overrides final {
 public:
  explicit Overrides(
      const Options& options,
      const Methods& methods,
      const DexStoresVector& stores);

  Overrides(const Overrides&) = delete;
  Overrides(Overrides&&) = delete;
  Overrides& operator=(const Overrides&) = delete;
  Overrides& operator=(Overrides&&) = delete;
  ~Overrides() = default;

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

  Json::Value to_json() const;

 private:
  UniquePointerConcurrentMap<const Method*, std::unordered_set<const Method*>>
      overrides_;
  std::unordered_set<const Method*> empty_method_set_;
};

} // namespace marianatrench

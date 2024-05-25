/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_map>

#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Registry.h>

namespace marianatrench {

/**
 * Stores cached models and other associated data to be parsed from an input
 * directory. Other data includes class hierarchy information. The cached input
 * may contain methods not defined in the current APK. Their corresponding
 * class hierarchy is also absent from the current APK. The analysis needs that
 * information to join overriding methods correctly at call-sites.
 */
class CachedModelsContext final {
 public:
  CachedModelsContext(Context& context, const Options& options);

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(CachedModelsContext)

  const std::unordered_map<const Method*, std::unordered_set<const Method*>>&
  overrides() const {
    mt_assert(!is_cleared_);
    return overrides_;
  }

  const std::unordered_map<const DexType*, std::unordered_set<const DexType*>>&
  class_hierarchy() const {
    mt_assert(!is_cleared_);
    return class_hierarchy_;
  }

  const std::optional<Registry>& models() const {
    mt_assert(!is_cleared_);
    return models_;
  }

  /**
   * Clears the cache if no longer in use. Frees up memory. Do not access the
   * cache once it is cleared.
   */
  void clear();

 private:
  std::unordered_map<const Method*, std::unordered_set<const Method*>>
      overrides_;
  std::unordered_map<const DexType*, std::unordered_set<const DexType*>>
      class_hierarchy_;
  std::optional<Registry> models_;
  bool is_cleared_;
};

} // namespace marianatrench

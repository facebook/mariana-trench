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
#include <mariana-trench/Options.h>
#include <mariana-trench/UniquePointerConcurrentMap.h>

namespace marianatrench {

class ClassHierarchies final {
 public:
  explicit ClassHierarchies(
      const Options& options,
      const DexStoresVector& stores);

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ClassHierarchies)

  /* Return the set of classes that extend the given class. */
  const std::unordered_set<const DexType*>& extends(const DexType* klass) const;

  Json::Value to_json() const;

 private:
  UniquePointerConcurrentMap<const DexType*, std::unordered_set<const DexType*>>
      extends_;
  std::unordered_set<const DexType*> empty_type_set_;
};

} // namespace marianatrench

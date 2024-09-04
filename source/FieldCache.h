/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_map>

#include <DexStore.h>

#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/UniquePointerConcurrentMap.h>

namespace marianatrench {

class FieldCache final {
 public:
  using Types = std::unordered_set<const DexType*>;

 private:
  using FieldTypeMap = std::unordered_map<const DexString*, Types>;

 public:
  explicit FieldCache(
      const ClassHierarchies& class_hierarchies,
      const DexStoresVector& stores);

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(FieldCache)

  /**
   * Returns the possible types of `field` in `klass`.
   * This includes fields that may be present in any class in the hierarchy of
   * `klass` (ancestors and descendents).
   */
  const Types& field_types(const DexType* klass, const DexString* field) const;

  /**
   * Returns true iff there is some field entry for the class in the cache. The
   * absence of a field entry (i.e. return value false) indicates that there is
   * insufficient information about `klass` for `field_types(...)` to be useful.
   */
  bool has_class_info(const DexType* klass) const;

 private:
  UniquePointerConcurrentMap<const DexType*, FieldTypeMap> field_cache_;
  Types empty_types_;
};

} // namespace marianatrench

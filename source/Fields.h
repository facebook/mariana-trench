/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_map>

#include <DexStore.h>

#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/UniquePointerConcurrentMap.h>

namespace marianatrench {

class Fields final {
 public:
  using Types = std::unordered_set<const DexType*>;

 private:
  using FieldTypeMap = std::unordered_map<const DexString*, Types>;

 public:
  explicit Fields(
      const ClassHierarchies& class_hierarchies,
      const DexStoresVector& stores);

  Fields(const Fields&) = delete;
  Fields(Fields&&) = delete;
  Fields& operator=(const Fields&) = delete;
  Fields& operator=(Fields&&) = delete;
  ~Fields() = default;

  /**
   * Returns the possible types of `field` in `klass`.
   * This includes fields that may be present in any class in the hierarchy of
   * `klass` (ancestors and descendents).
   */
  const Types& field_types(const DexType* klass, const DexString* field) const;

 private:
  UniquePointerConcurrentMap<const DexType*, FieldTypeMap> fields_;
  Types empty_types_;
};

} // namespace marianatrench

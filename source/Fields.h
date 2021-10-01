/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_map>

#include <DexStore.h>

#include <mariana-trench/Compiler.h>
#include <mariana-trench/UniquePointerConcurrentMap.h>

namespace marianatrench {

class Fields final {
 private:
  using FieldNameToTypeMap =
      std::unordered_map<const DexString*, const DexType*>;

 public:
  explicit Fields(const DexStoresVector& stores);

  Fields(const Fields&) = delete;
  Fields(Fields&&) = delete;
  Fields& operator=(const Fields&) = delete;
  Fields& operator=(Fields&&) = delete;
  ~Fields() = default;

  /**
   * Return the type of field in `klass`, nullptr if field does not exist.
   */
  const DexType* MT_NULLABLE
  field_type(const DexType* klass, const DexString* field) const;

 private:
  UniquePointerConcurrentMap<const DexType*, FieldNameToTypeMap> fields_;
};

} // namespace marianatrench

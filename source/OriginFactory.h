/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Origin.h>
#include <mariana-trench/TupleHash.h>
#include <mariana-trench/UniquePointerFactory.h>

namespace marianatrench {

class OriginFactory final {
 private:
  OriginFactory() = default;

 public:
  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(OriginFactory)

  const MethodOrigin* method_origin(
      const Method* method,
      const AccessPath* port) const;

  const FieldOrigin* field_origin(const Field* field) const;

  const CrtexOrigin* crtex_origin(
      std::string_view canonical_name,
      const AccessPath* port) const;

  const StringOrigin* string_origin(std::string_view name) const;

  static const OriginFactory& singleton();

 private:
  UniquePointerFactory<
      std::tuple<const Method*, const AccessPath*>,
      MethodOrigin,
      TupleHash<const Method*, const AccessPath*>>
      method_origins_;
  UniquePointerFactory<const Field*, FieldOrigin> field_origins_;
  UniquePointerFactory<
      std::tuple<const DexString*, const AccessPath*>,
      CrtexOrigin,
      TupleHash<const DexString*, const AccessPath*>>
      crtex_origins_;
  UniquePointerFactory<const DexString*, StringOrigin> string_origins_;
};

} // namespace marianatrench

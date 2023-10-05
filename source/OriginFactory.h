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

  const MethodOrigin* method_origin(const Method* method) const;

  const FieldOrigin* field_origin(const Field* field) const;

  static const OriginFactory& singleton();

 private:
  // TODO(T163918472): Include port in method origins
  // UniquePointerFactory<
  //     std::tuple<const Method*, const AccessPath*>,
  //     MethodOrigin,
  //     TupleHash<const Method*, const AccessPath*>>
  //     method_origins_;

  UniquePointerFactory<const Method*, MethodOrigin> method_origins_;
  UniquePointerFactory<const Field*, FieldOrigin> field_origins_;
};

} // namespace marianatrench

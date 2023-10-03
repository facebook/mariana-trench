/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ConcurrentContainers.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

class AccessPathFactory final {
 private:
  AccessPathFactory() = default;

 public:
  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(AccessPathFactory)

  const AccessPath* get(const AccessPath& access_path) const;

  static const AccessPathFactory& singleton();

 private:
  mutable InsertOnlyConcurrentSet<AccessPath> access_paths_;
};

} // namespace marianatrench

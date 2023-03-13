/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/shim-generator/Shim.h>

namespace marianatrench {

using MethodToShimMap = std::unordered_map<const Method*, Shim>;

class Shims final {
 public:
  explicit Shims(MethodToShimMap global_shims)
      : global_shims_(std::move(global_shims)) {}

  explicit Shims() {
    global_shims_ = MethodToShimMap{};
  }

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Shims)
  std::optional<Shim> get_shim_for_caller(
      const Method* original_callee,
      const Method* caller) const;

 private:
  MethodToShimMap global_shims_;
};

} // namespace marianatrench

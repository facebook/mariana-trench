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

class Shims final {
 private:
  using MethodToShimMap = std::unordered_map<const Method*, Shim>;

 public:
  explicit Shims(std::size_t global_shims_size)
      : global_shims_(global_shims_size) {}

  explicit Shims() = default;

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Shims)

  std::optional<Shim> get_shim_for_caller(
      const Method* original_callee,
      const Method* caller) const;

  bool add_global_method_shim(const Shim& shim);

 private:
  MethodToShimMap global_shims_;
};

} // namespace marianatrench

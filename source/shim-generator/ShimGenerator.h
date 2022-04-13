/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Methods.h>

namespace marianatrench {

struct ShimTarget;
using ShimParameterPosition = std::uint32_t;
using MethodToShimTargetsMap =
    std::unordered_map<const Method*, std::vector<ShimTarget>>;

struct ShimTarget {
  const Method* call_target;
  std::optional<ShimParameterPosition> receiver_position_in_shim;
  std::vector<ShimParameterPosition> parameter_positions_in_shim;

  static std::optional<ShimTarget> try_create(
      const Method* call_target,
      const std::unordered_map<const DexType*, ParameterPosition>&
          parameter_types_to_position);
};

class ShimGenerator {
 public:
  explicit ShimGenerator(const std::string& name);

  ShimGenerator(const ShimGenerator&) = delete;
  ShimGenerator(ShimGenerator&&) = default;
  ShimGenerator& operator=(const ShimGenerator&) = delete;
  ShimGenerator& operator=(ShimGenerator&&) = delete;
  ~ShimGenerator() = default;

  MethodToShimTargetsMap emit_artificial_callees(const Methods& methods) const;

 private:
  std::string name_;
};

} // namespace marianatrench

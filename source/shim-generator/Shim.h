/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>

#include <mariana-trench/Access.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Method.h>

namespace marianatrench {

class Shim;
using ShimParameterPosition = ParameterPosition;
using MethodToShimMap = std::unordered_map<const Method*, Shim>;

class ShimMethod {
 public:
  explicit ShimMethod(const Method* method);

  DexType* MT_NULLABLE parameter_type(ShimParameterPosition argument) const;
  std::optional<ShimParameterPosition> type_position(const DexType* type) const;
  std::vector<ShimParameterPosition> method_parameter_positions(
      const Method* method) const;

 private:
  const Method* method_;
  std::unordered_map<const DexType*, ShimParameterPosition> types_to_position_;
};

class ShimTarget {
 public:
  explicit ShimTarget(
      const Method* method,
      std::optional<ShimParameterPosition> receiver_position = std::nullopt,
      std::vector<ShimParameterPosition> parameter_positions = {});

  static std::optional<ShimTarget> from_json(
      const Json::Value& value,
      Context& context);

  std::optional<ShimTarget> instantiate(const ShimMethod& shim_method) const;

  const Method* method() const {
    return call_target_;
  }

  std::optional<Register> receiver_register(
      const IRInstruction* instruction) const;

  std::vector<Register> parameter_registers(
      const IRInstruction* instruction) const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const ShimTarget& shim_target);

 private:
  const Method* call_target_;
  std::optional<ShimParameterPosition> receiver_position_in_shim_;
  std::vector<ShimParameterPosition> parameter_positions_in_shim_;
};

class Shim {
 public:
  Shim(const Method* method, std::vector<ShimTarget> shim_targets);

  const Method* method() const {
    return method_;
  }

  bool empty() const {
    return targets_.empty();
  }

  const std::vector<ShimTarget>& targets() const {
    return targets_;
  }

  friend std::ostream& operator<<(std::ostream& out, const Shim& shim);

 private:
  const Method* method_;
  std::vector<ShimTarget> targets_;
};

} // namespace marianatrench

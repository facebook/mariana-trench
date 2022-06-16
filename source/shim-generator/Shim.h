/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>
#include <variant>

#include <mariana-trench/Access.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Method.h>

namespace marianatrench {

class Shim;
/* Indicates the position of a parameter in the `shimmed-method`. */
using ShimParameterPosition = ParameterPosition;
using MethodToShimMap = std::unordered_map<const Method*, Shim>;

/**
 * Wrapper around the `shimmed-method` (i.e. the method matching the method
 * constraints on the shim generator) with helper methods to query
 * dex-types/positions parameters.
 */
class ShimMethod {
 public:
  explicit ShimMethod(const Method* method);

  const Method* method() const;
  DexType* MT_NULLABLE parameter_type(ShimParameterPosition argument) const;
  std::optional<ShimParameterPosition> type_position(const DexType* type) const;

 private:
  const Method* method_;
  // Maps parameter type to position in method_
  std::unordered_map<const DexType*, ShimParameterPosition> types_to_position_;
};

/**
 * Tracks the mapping of parameter positions from
 * `shim-target` (`ParameterPosition`) to
 * parameter positions in the `shimmed-method` (`ShimParameterPosition`)
 */
class ShimParameterMapping {
 public:
  ShimParameterMapping();

  static ShimParameterMapping from_json(const Json::Value& value);

  ShimParameterMapping instantiate(
      std::string_view shim_target_method,
      const DexType* shim_target_class,
      const DexProto* shim_target_proto,
      bool shim_target_is_static,
      const ShimMethod& shim_method) const;

  bool empty() const;
  bool contains(ParameterPosition position) const;
  std::optional<ShimParameterPosition> at(
      ParameterPosition parameter_position) const;
  void insert(
      ParameterPosition parameter_position,
      ShimParameterPosition shim_parameter_position);

  friend std::ostream& operator<<(
      std::ostream& out,
      const ShimParameterMapping& map);

 private:
  std::unordered_map<ParameterPosition, ShimParameterPosition> map_;
};

/**
 * Represents shim-targets which are instance methods.
 */
class ShimTarget {
 public:
  explicit ShimTarget(
      const Method* method,
      ShimParameterMapping parameter_mapping);

  const Method* method() const {
    return call_target_;
  }

  std::optional<Register> receiver_register(
      const IRInstruction* instruction) const;

  std::unordered_map<ParameterPosition, Register> parameter_registers(
      const IRInstruction* instruction) const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const ShimTarget& shim_target);

 private:
  const Method* call_target_;
  ShimParameterMapping parameter_mapping_;
};

/**
 * Represents shim-targets which are resolved using reflection.
 */
class ShimReflectionTarget {
 public:
  explicit ShimReflectionTarget(
      DexMethodSpec method_spec,
      ShimParameterMapping parameter_mapping);

  const DexMethodSpec& method_spec() const {
    return method_spec_;
  }

  std::optional<Register> receiver_register(
      const IRInstruction* instruction) const;

  std::unordered_map<ParameterPosition, Register> parameter_registers(
      const Method* resolved_reflection,
      const IRInstruction* instruction) const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const ShimReflectionTarget& shim_reflection_target);

 private:
  DexMethodSpec method_spec_;
  ShimParameterMapping parameter_mapping_;
};

using ShimTargetVariant = std::variant<ShimTarget, ShimReflectionTarget>;

/**
 * Represents an instantiated Shim for one `shimmed-method`.
 */
class Shim {
 public:
  explicit Shim(const Method* method);

  void add_target(ShimTargetVariant target);

  const Method* method() const {
    return method_;
  }

  bool empty() const {
    return targets_.empty() && reflections_.empty();
  }

  const std::vector<ShimTarget>& targets() const {
    return targets_;
  }

  const std::vector<ShimReflectionTarget>& reflections() const {
    return reflections_;
  }

  friend std::ostream& operator<<(std::ostream& out, const Shim& shim);

 private:
  const Method* method_;
  std::vector<ShimTarget> targets_;
  std::vector<ShimReflectionTarget> reflections_;
};

} // namespace marianatrench

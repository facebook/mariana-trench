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

/* Indicates the position of a parameter in the `shimmed-method`. */
using ShimParameterPosition = ParameterPosition;

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

  friend std::ostream& operator<<(
      std::ostream& out,
      const ShimMethod& shim_method);

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

  static ShimParameterMapping from_json(
      const Json::Value& value,
      bool infer_from_types);

  bool empty() const;
  bool contains(ParameterPosition position) const;
  std::optional<ShimParameterPosition> at(
      ParameterPosition parameter_position) const;
  void insert(
      ParameterPosition parameter_position,
      ShimParameterPosition shim_parameter_position);

  void set_infer_from_types(bool value);
  bool infer_from_types() const;

  void add_receiver(ShimParameterPosition shim_parameter_position);

  ShimParameterMapping instantiate_parameters(
      std::string_view shim_target_method,
      const DexType* shim_target_class,
      const DexProto* shim_target_proto,
      bool shim_target_is_static,
      const ShimMethod& shim_method) const;

  void infer_parameters_from_types(
      const DexProto* shim_target_proto,
      bool shim_target_is_static,
      const ShimMethod& shim_method);

  friend std::ostream& operator<<(
      std::ostream& out,
      const ShimParameterMapping& map);

 private:
  std::unordered_map<ParameterPosition, ShimParameterPosition> map_;
  bool infer_from_types_;
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

/**
 * Represents shim-targets which are the generated lifecycle wrappers.
 * The target lifecycle wrapper method is resolved at the call-site only
 * as each generated wrapper has a unique signature.
 */
class ShimLifecycleTarget {
 public:
  explicit ShimLifecycleTarget(
      std::string method_name,
      ShimParameterPosition receiver_position,
      bool is_reflection,
      bool infer_parameter_mapping);

  std::string_view method_name() const {
    return method_name_;
  }

  Register receiver_register(const IRInstruction* instruction) const;

  std::unordered_map<ParameterPosition, Register> parameter_registers(
      const Method* callee,
      const Method* lifecycle_method,
      const IRInstruction* instruction) const;

  bool is_reflection() const {
    return is_reflection_;
  }

  bool infer_from_types() const {
    return infer_from_types_;
  }

  friend std::ostream& operator<<(
      std::ostream& out,
      const ShimLifecycleTarget& shim_lifecycle_target);

 private:
  std::string method_name_;
  ShimParameterPosition receiver_position_;
  bool is_reflection_;
  bool infer_from_types_;
};

using ShimTargetVariant =
    std::variant<ShimTarget, ShimReflectionTarget, ShimLifecycleTarget>;

/**
 * Represents an instantiated Shim for one `shimmed-method`.
 */
class InstantiatedShim {
 public:
  explicit InstantiatedShim(const Method* method);

  void add_target(ShimTargetVariant target);

  const Method* method() const {
    return method_;
  }

  bool empty() const {
    return targets_.empty() && reflections_.empty() && lifecycles_.empty();
  }

  const std::vector<ShimTarget>& targets() const {
    return targets_;
  }

  const std::vector<ShimReflectionTarget>& reflections() const {
    return reflections_;
  }

  const std::vector<ShimLifecycleTarget>& lifecycles() const {
    return lifecycles_;
  }

  friend std::ostream& operator<<(
      std::ostream& out,
      const InstantiatedShim& shim);

 private:
  const Method* method_;
  std::vector<ShimTarget> targets_;
  std::vector<ShimReflectionTarget> reflections_;
  std::vector<ShimLifecycleTarget> lifecycles_;
};

class Shim {
 public:
  explicit Shim(
      const InstantiatedShim* MT_NULLABLE instantiated_shim,
      std::vector<ShimTarget> intent_routing_targets);

  const Method* method() const {
    return instantiated_shim_->method();
  }

  bool empty() const {
    return instantiated_shim_->empty() && intent_routing_targets_.empty();
  }

  const std::vector<ShimTarget>& targets() const {
    if (instantiated_shim_ == nullptr) {
      return Shim::empty_shim_targets();
    }
    return instantiated_shim_->targets();
  }

  const std::vector<ShimReflectionTarget>& reflections() const {
    if (instantiated_shim_ == nullptr) {
      return Shim::empty_reflection_targets();
    }
    return instantiated_shim_->reflections();
  }

  const std::vector<ShimLifecycleTarget>& lifecycles() const {
    if (instantiated_shim_ == nullptr) {
      return Shim::empty_lifecycle_targets();
    }
    return instantiated_shim_->lifecycles();
  }

  const std::vector<ShimTarget>& intent_routing_targets() const {
    return intent_routing_targets_;
  }

  static const std::vector<ShimTarget>& empty_shim_targets() {
    static const std::vector<ShimTarget> empty;
    return empty;
  }
  static const std::vector<ShimLifecycleTarget>& empty_lifecycle_targets() {
    static const std::vector<ShimLifecycleTarget> empty;
    return empty;
  }
  static const std::vector<ShimReflectionTarget>& empty_reflection_targets() {
    static const std::vector<ShimReflectionTarget> empty;
    return empty;
  }

  friend std::ostream& operator<<(std::ostream& out, const Shim& shim);

 private:
  const InstantiatedShim* instantiated_shim_;
  std::vector<ShimTarget> intent_routing_targets_;
};

} // namespace marianatrench

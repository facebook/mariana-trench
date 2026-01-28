/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <initializer_list>
#include <optional>
#include <variant>

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/container_hash/hash_fwd.hpp>

#include <DexMemberRefs.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Method.h>

namespace marianatrench {

/* Indicates the position of a parameter in the `shimmed-method`. */
using ShimRoot = Root;

/**
 * Wrapper around the `shimmed-method` (i.e. the method matching the method
 * constraints on the shim generator) with helper methods to query
 * dex-types/positions parameters.
 */
class ShimMethod final {
 public:
  explicit ShimMethod(const Method* method);

  const Method* method() const;

  const DexType* MT_NULLABLE parameter_type(ShimRoot argument) const;

  DexType* MT_NULLABLE return_type() const;

  bool is_valid_port(ShimRoot port) const;

  std::optional<ShimRoot> type_position(const DexType* type) const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const ShimMethod& shim_method);

 private:
  const Method* method_;
  // Maps parameter type to position in method_
  boost::container::flat_map<const DexType*, ShimRoot> types_to_position_;
};

/**
 * Tracks the port mapping between `shim-target` and `shimmed-method`.
 *
 * - map_: Tracks the mapping of parameter positions from `shim-target` (`Root`)
 * to parameter positions / return ports in the `shimmed-method` (`ShimRoot`).
 * This is used for retrieving the arguments for the call to the shim-target
 * from the arguments/return value of shimmed-method.
 *
 * - return_to_: This tracks the shimmed-method's port where the return value of
 * the call to the shim-target will be forwarded.
 */
class ShimTargetPortMapping final {
 public:
  using MapType = boost::container::flat_map<Root, ShimRoot>;

 public:
  explicit ShimTargetPortMapping(
      std::initializer_list<MapType::value_type> init = {});

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ShimTargetPortMapping)

  bool operator==(const ShimTargetPortMapping& other) const;

  bool operator<(const ShimTargetPortMapping& other) const;

  static ShimTargetPortMapping from_json(
      const Json::Value& value,
      bool infer_from_types,
      std::optional<ShimRoot> return_to = std::nullopt);

  bool empty() const;

  bool contains(Root position) const;

  std::optional<ShimRoot> at(Root parameter_position) const;

  void insert(Root parameter_position, ShimRoot shim_parameter_position);

  MapType::const_iterator begin() const {
    return map_.begin();
  }

  MapType::const_iterator end() const {
    return map_.end();
  }

  void set_infer_from_types(bool value);

  bool infer_from_types() const;

  void add_receiver(ShimRoot shim_parameter_position);

  void remove_receiver();

  ShimTargetPortMapping instantiate(
      std::string_view shim_target_method,
      const DexType* shim_target_class,
      const DexProto* shim_target_proto,
      bool shim_target_is_static,
      const ShimMethod& shim_method) const;

  void infer_parameters_from_types(
      const DexProto* shim_target_proto,
      bool shim_target_is_static,
      const ShimMethod& shim_method);

  const std::optional<ShimRoot>& return_to() const;

  void set_return_to(std::optional<ShimRoot> return_to);

  friend std::ostream& operator<<(
      std::ostream& out,
      const ShimTargetPortMapping& map);

  friend std::hash<ShimTargetPortMapping>;

 private:
  MapType map_;
  bool infer_from_types_;
  std::optional<ShimRoot> return_to_;
};

/**
 * Represents shim-target methods with static or instance receiver kinds.
 */
class ShimTarget final {
 public:
  explicit ShimTarget(
      DexMethodSpec method_spec,
      ShimTargetPortMapping port_mapping,
      bool is_static);

  explicit ShimTarget(const Method* method, ShimTargetPortMapping port_mapping);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ShimTarget)

  bool operator==(const ShimTarget& other) const;

  bool operator<(const ShimTarget& other) const;

  const DexMethodSpec& method_spec() const {
    return method_spec_;
  }

  bool is_static() const {
    return is_static_;
  }

  std::optional<Register> receiver_register(
      const IRInstruction* instruction) const;

  std::unordered_map<Root, Register> root_registers(
      const IRInstruction* instruction) const;

  std::optional<Register> return_to_register(
      const IRInstruction* instruction) const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const ShimTarget& shim_target);

  friend struct std::hash<ShimTarget>;

  const std::optional<ShimRoot>& return_to() const {
    return port_mapping_.return_to();
  }

 private:
  DexMethodSpec method_spec_;
  ShimTargetPortMapping port_mapping_;
  bool is_static_;
};

/**
 * Represents shim-target methods whose receiver types are resolved using
 * reflection.
 */
class ShimReflectionTarget final {
 public:
  /**
   * Constructs an unresolved shim reflection target where the receiver class in
   * the DexMethodSpec is always java.lang.Class. Once the receiver (hence the
   * shim-target method) is resolved at the callsite, a resolved shim reflection
   * target can be created (see resolve() below) with the appropriate receiver
   * type.
   */
  explicit ShimReflectionTarget(
      DexMethodSpec method_spec,
      ShimTargetPortMapping port_mapping);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ShimReflectionTarget)

 private:
  /* Constructs a resolved shim reflection target using the
   * resolved_reflection_method */
  explicit ShimReflectionTarget(
      const Method* resolved_reflection_method,
      ShimTargetPortMapping instantiated_port_mapping);

 public:
  ShimReflectionTarget resolve(
      const Method* shimmed_callee,
      const Method* resolved_reflection) const;

  bool operator==(const ShimReflectionTarget& other) const;

  bool operator<(const ShimReflectionTarget& other) const;

  const DexMethodSpec& method_spec() const {
    return method_spec_;
  }

  Register receiver_register(const IRInstruction* instruction) const;

  std::unordered_map<Root, Register> root_registers(
      const IRInstruction* instruction) const;

  std::optional<Register> return_to_register(
      const IRInstruction* instruction) const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const ShimReflectionTarget& shim_reflection_target);

  friend struct std::hash<ShimReflectionTarget>;

 private:
  DexMethodSpec method_spec_;
  ShimTargetPortMapping port_mapping_;
  bool is_resolved_;
};

/**
 * Represents shim-target methods which are the generated lifecycle wrappers.
 * The target lifecycle wrapper method is resolved at the call-site only
 * as each generated wrapper has a unique signature.
 */
class ShimLifecycleTarget final {
 public:
  explicit ShimLifecycleTarget(
      std::string method_name,
      ShimRoot receiver_position,
      bool is_reflection,
      bool infer_from_types);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ShimLifecycleTarget)

  bool operator==(const ShimLifecycleTarget& other) const;

  bool operator<(const ShimLifecycleTarget& other) const;

  const std::string& method_name() const {
    return method_name_;
  }

  Register receiver_register(const IRInstruction* instruction) const;

  std::unordered_map<Root, Register> root_registers(
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

  friend struct std::hash<ShimLifecycleTarget>;

 private:
  std::string method_name_;
  ShimRoot receiver_position_;
  bool is_reflection_;
  bool infer_from_types_;
};

using ShimTargetVariant =
    std::variant<ShimTarget, ShimReflectionTarget, ShimLifecycleTarget>;

} // namespace marianatrench

template <>
struct std::hash<marianatrench::ShimTargetPortMapping> {
  std::size_t operator()(
      const marianatrench::ShimTargetPortMapping& port_mapping) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, port_mapping.infer_from_types_);
    for (const auto& [root, shim_root] : port_mapping.map_) {
      boost::hash_combine(seed, std::hash<marianatrench::Root>()(root));
      boost::hash_combine(
          seed, std::hash<marianatrench::ShimRoot>()(shim_root));
    }
    if (port_mapping.return_to_.has_value()) {
      boost::hash_combine(
          seed, std::hash<marianatrench::ShimRoot>()(*port_mapping.return_to_));
    }
    return seed;
  }
};

template <>
struct std::hash<marianatrench::ShimTarget> {
  std::size_t operator()(const marianatrench::ShimTarget& shim_target) const {
    std::size_t seed = 0;
    boost::hash_combine(
        seed, std::hash<DexMethodSpec>()(shim_target.method_spec_));
    boost::hash_combine(
        seed,
        std::hash<marianatrench::ShimTargetPortMapping>()(
            shim_target.port_mapping_));
    boost::hash_combine(seed, shim_target.is_static_);
    return seed;
  }
};

template <>
struct std::hash<marianatrench::ShimReflectionTarget> {
  std::size_t operator()(
      const marianatrench::ShimReflectionTarget& shim_reflection_target) const {
    std::size_t seed = 0;
    boost::hash_combine(
        seed, std::hash<DexMethodSpec>()(shim_reflection_target.method_spec_));
    boost::hash_combine(
        seed,
        std::hash<marianatrench::ShimTargetPortMapping>()(
            shim_reflection_target.port_mapping_));
    boost::hash_combine(seed, shim_reflection_target.is_resolved_);
    return seed;
  }
};

template <>
struct std::hash<marianatrench::ShimLifecycleTarget> {
  std::size_t operator()(
      const marianatrench::ShimLifecycleTarget& shim_lifecycle_target) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, shim_lifecycle_target.method_name_);
    boost::hash_combine(
        seed,
        std::hash<marianatrench::ShimRoot>()(
            shim_lifecycle_target.receiver_position_));
    boost::hash_combine(seed, shim_lifecycle_target.is_reflection_);
    boost::hash_combine(seed, shim_lifecycle_target.infer_from_types_);
    return seed;
  }
};

namespace marianatrench {

/**
 * Represents an instantiated Shim for one `shimmed-method`.
 */
class InstantiatedShim final {
 public:
  template <typename Element>
  using FlatSet = boost::container::flat_set<Element>;

 public:
  explicit InstantiatedShim(const Method* method);

  MOVE_CONSTRUCTOR_ONLY(InstantiatedShim)

  void add_target(ShimTargetVariant target);

  void merge_with(InstantiatedShim other);

  const Method* method() const {
    return method_;
  }

  bool empty() const {
    return targets_.empty() && reflections_.empty() && lifecycles_.empty();
  }

  const FlatSet<ShimTarget>& targets() const {
    return targets_;
  }

  const FlatSet<ShimReflectionTarget>& reflections() const {
    return reflections_;
  }

  const FlatSet<ShimLifecycleTarget>& lifecycles() const {
    return lifecycles_;
  }

  friend std::ostream& operator<<(
      std::ostream& out,
      const InstantiatedShim& shim);

 private:
  const Method* method_;
  FlatSet<ShimTarget> targets_;
  FlatSet<ShimReflectionTarget> reflections_;
  FlatSet<ShimLifecycleTarget> lifecycles_;
};

class Shim final {
 public:
  explicit Shim(
      const InstantiatedShim* MT_NULLABLE instantiated_shim,
      InstantiatedShim::FlatSet<ShimTarget> intent_routing_targets);

  const Method* MT_NULLABLE method() const {
    return instantiated_shim_ ? instantiated_shim_->method() : nullptr;
  }

  bool empty() const {
    return (instantiated_shim_ == nullptr || instantiated_shim_->empty()) &&
        intent_routing_targets_.empty();
  }

  const InstantiatedShim::FlatSet<ShimTarget>& targets() const;

  const InstantiatedShim::FlatSet<ShimReflectionTarget>& reflections() const;

  const InstantiatedShim::FlatSet<ShimLifecycleTarget>& lifecycles() const;

  const InstantiatedShim::FlatSet<ShimTarget>& intent_routing_targets() const {
    return intent_routing_targets_;
  }

  friend std::ostream& operator<<(std::ostream& out, const Shim& shim);

 private:
  const InstantiatedShim* MT_NULLABLE instantiated_shim_;
  InstantiatedShim::FlatSet<ShimTarget> intent_routing_targets_;
};

} // namespace marianatrench

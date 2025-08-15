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
using ShimParameterPosition = ParameterPosition;

/**
 * Wrapper around the `shimmed-method` (i.e. the method matching the method
 * constraints on the shim generator) with helper methods to query
 * dex-types/positions parameters.
 */
class ShimMethod final {
 public:
  explicit ShimMethod(const Method* method);

  const Method* method() const;

  DexType* MT_NULLABLE parameter_type(ShimParameterPosition argument) const;

  DexType* MT_NULLABLE return_type() const;

  std::optional<ShimParameterPosition> type_position(const DexType* type) const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const ShimMethod& shim_method);

 private:
  const Method* method_;
  // Maps parameter type to position in method_
  boost::container::flat_map<const DexType*, ShimParameterPosition>
      types_to_position_;
};

/**
 * Tracks the mapping of parameter positions from
 * `shim-target` (`ParameterPosition`) to
 * parameter positions in the `shimmed-method` (`ShimParameterPosition`)
 */
class ShimParameterMapping final {
 public:
  using MapType = boost::container::flat_map<Root, ShimParameterPosition>;

 public:
  explicit ShimParameterMapping(
      std::initializer_list<MapType::value_type> init = {});

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ShimParameterMapping)

  bool operator==(const ShimParameterMapping& other) const;

  bool operator<(const ShimParameterMapping& other) const;

  static ShimParameterMapping from_json(
      const Json::Value& value,
      bool infer_from_types);

  bool empty() const;

  bool contains(const Root& position) const;

  std::optional<ShimParameterPosition> at(const Root& parameter_position) const;

  void insert(
      const Root& parameter_position,
      ShimParameterPosition shim_parameter_position);

  MapType::const_iterator begin() const {
    return map_.begin();
  }

  MapType::const_iterator end() const {
    return map_.end();
  }

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

  friend std::hash<ShimParameterMapping>;

 private:
  MapType map_;
  bool infer_from_types_;
};

/**
 * Represents shim-target methods with static or instance receiver kinds.
 */
class ShimTarget final {
 public:
  explicit ShimTarget(
      DexMethodSpec method_spec,
      ShimParameterMapping parameter_mapping,
      bool is_static);

  explicit ShimTarget(
      const Method* method,
      ShimParameterMapping parameter_mapping);

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

  friend std::ostream& operator<<(
      std::ostream& out,
      const ShimTarget& shim_target);

  friend struct std::hash<ShimTarget>;

 private:
  DexMethodSpec method_spec_;
  ShimParameterMapping parameter_mapping_;
  bool is_static_;
};

/**
 * Represents shim-target methods whose receiver types are resolved using
 * reflection.
 */
class ShimReflectionTarget final {
 public:
  explicit ShimReflectionTarget(
      DexMethodSpec method_spec,
      ShimParameterMapping parameter_mapping);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ShimReflectionTarget)

  bool operator==(const ShimReflectionTarget& other) const;

  bool operator<(const ShimReflectionTarget& other) const;

  const DexMethodSpec& method_spec() const {
    return method_spec_;
  }

  Register receiver_register(const IRInstruction* instruction) const;

  std::unordered_map<Root, Register> root_registers(
      const Method* resolved_reflection,
      const IRInstruction* instruction) const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const ShimReflectionTarget& shim_reflection_target);

  friend struct std::hash<ShimReflectionTarget>;

 private:
  DexMethodSpec method_spec_;
  ShimParameterMapping parameter_mapping_;
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
      ShimParameterPosition receiver_position,
      bool is_reflection,
      bool infer_parameter_mapping);

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
  ShimParameterPosition receiver_position_;
  bool is_reflection_;
  bool infer_from_types_;
};

using ShimTargetVariant =
    std::variant<ShimTarget, ShimReflectionTarget, ShimLifecycleTarget>;

} // namespace marianatrench

template <>
struct std::hash<marianatrench::ShimParameterMapping> {
  std::size_t operator()(
      const marianatrench::ShimParameterMapping& shim_parameter_mapping) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, shim_parameter_mapping.infer_from_types_);
    for (const auto& [root, position] : shim_parameter_mapping.map_) {
      boost::hash_combine(seed, std::hash<marianatrench::Root>()(root));
      boost::hash_combine(seed, position);
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
        std::hash<marianatrench::ShimParameterMapping>()(
            shim_target.parameter_mapping_));
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
        std::hash<marianatrench::ShimParameterMapping>()(
            shim_reflection_target.parameter_mapping_));
    return seed;
  }
};

template <>
struct std::hash<marianatrench::ShimLifecycleTarget> {
  std::size_t operator()(
      const marianatrench::ShimLifecycleTarget& shim_lifecycle_target) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, shim_lifecycle_target.method_name_);
    boost::hash_combine(seed, shim_lifecycle_target.receiver_position_);
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

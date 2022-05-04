/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <boost/iterator/filter_iterator.hpp>
#include <boost/range/iterator_range.hpp>
#include <json/json.h>

#include <ConcurrentContainers.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/Feature.h>
#include <mariana-trench/FeatureSet.h>
#include <mariana-trench/Field.h>
#include <mariana-trench/Fields.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/Types.h>
#include <mariana-trench/shim-generator/ShimGenerator.h>

namespace marianatrench {

using TextualOrderIndex = std::uint32_t;

/**
 * Represents information about a specific call.
 */
class CallTarget final {
 private:
  struct FilterOverrides {
    bool operator()(const Method*) const;

    const std::unordered_set<const DexType*>* MT_NULLABLE extends;
  };

 public:
  using OverridesRange = boost::iterator_range<boost::filter_iterator<
      FilterOverrides,
      std::unordered_set<const Method*>::const_iterator>>;

 public:
  static CallTarget static_call(
      const IRInstruction* instruction,
      const Method* MT_NULLABLE callee,
      TextualOrderIndex call_index);

  static CallTarget virtual_call(
      const IRInstruction* instruction,
      const Method* MT_NULLABLE resolved_base_callee,
      TextualOrderIndex call_index,
      const DexType* MT_NULLABLE receiver_type,
      const ClassHierarchies& class_hierarchies,
      const Overrides& override_factory);

  static CallTarget from_call_instruction(
      const Method* caller,
      const IRInstruction* instruction,
      const Method* MT_NULLABLE resolved_base_callee,
      TextualOrderIndex call_index,
      const Types& types,
      const ClassHierarchies& class_hierarchies,
      const Overrides& override_factory);

  CallTarget(const CallTarget&) = default;
  CallTarget(CallTarget&&) = default;
  CallTarget& operator=(const CallTarget&) = default;
  CallTarget& operator=(CallTarget&&) = default;
  ~CallTarget() = default;

  /*
   * The instruction that triggered the call.
   * Note that this is not necessarily an invoke, see artificial calls.
   */
  const IRInstruction* instruction() const {
    return instruction_;
  }

  /* Return true if the call behaves like a virtual call. */
  bool is_virtual() const {
    return overrides_ != nullptr;
  }

  bool resolved() const {
    return resolved_base_callee_ != nullptr;
  }

  /**
   * For a static call, returns the callee.
   * For a virtual call, returns the resolved base callee. This is the base
   * method of all possible callees (i.e, all overrides). The method is
   * resolved, i.e if the method is not defined, we use the first defined
   * method in the closest parent class.
   *
   * For instance:
   * ```
   * class A { void f() { ... } }
   * class B implements A {}
   * class C extends B { void f() { ... } }
   * ```
   * A virtual call to `B::f` has a resolved base callee of `A::f`.
   */
  const Method* MT_NULLABLE resolved_base_callee() const {
    return resolved_base_callee_;
  }

  /* For a virtual call, returns the inferred type of `this`. */
  const DexType* MT_NULLABLE receiver_type() const {
    return receiver_type_;
  }

  TextualOrderIndex call_index() const {
    return call_index_;
  }

  /**
   * For a virtual call, returns all overriding methods that could be called.
   * This does not include the resolved base callee.
   */
  OverridesRange overrides() const;

  bool operator==(const CallTarget& other) const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const CallTarget& call_target);

  friend struct std::hash<CallTarget>;

 private:
  CallTarget(
      const IRInstruction* instruction,
      const Method* MT_NULLABLE resolved_base_callee,
      TextualOrderIndex call_index,
      const DexType* MT_NULLABLE receiver_type,
      const std::unordered_set<const Method*>* MT_NULLABLE overrides,
      const std::unordered_set<const DexType*>* MT_NULLABLE receiver_extends);

 private:
  const IRInstruction* instruction_;
  const Method* MT_NULLABLE resolved_base_callee_;
  TextualOrderIndex call_index_;
  const DexType* MT_NULLABLE receiver_type_;
  const std::unordered_set<const Method*>* MT_NULLABLE overrides_;
  const std::unordered_set<const DexType*>* MT_NULLABLE receiver_extends_;
};

} // namespace marianatrench

template <>
struct std::hash<marianatrench::CallTarget> {
  std::size_t operator()(const marianatrench::CallTarget& call_target) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, call_target.instruction_);
    boost::hash_combine(seed, call_target.resolved_base_callee_);
    boost::hash_combine(seed, call_target.call_index_);
    boost::hash_combine(seed, call_target.receiver_type_);
    boost::hash_combine(seed, call_target.overrides_);
    boost::hash_combine(seed, call_target.receiver_extends_);
    return seed;
  }
};

namespace marianatrench {

/**
 * Represents an artificial edge in the call graph.
 *
 * This is currently used to simulate calls to methods of anonymous classes
 * flowing through an external method.
 *
 * `parameter_registers` is the map of parameters from position to registers for
 * the artificial invoke instruction.
 */
struct ArtificialCallee {
  CallTarget call_target;
  std::unordered_map<ParameterPosition, Register> parameter_registers;
  FeatureSet features;

  bool operator==(const ArtificialCallee& other) const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const ArtificialCallee& callee);
};

using ArtificialCallees = std::vector<ArtificialCallee>;

class CallGraph final {
 public:
  explicit CallGraph(
      const Options& options,
      Methods& methods,
      Fields& fields,
      const Types& types,
      const ClassHierarchies& class_hierarchies,
      Overrides& overrides,
      const Features& features,
      const MethodToShimMap& shims);

  CallGraph(const CallGraph&) = delete;
  CallGraph(CallGraph&&) = delete;
  CallGraph& operator=(const CallGraph&) = delete;
  CallGraph& operator=(CallGraph&&) = delete;
  ~CallGraph() = default;

  /* Return all call targets for the given method. */
  std::vector<CallTarget> callees(const Method* caller) const;

  /* Return the call target for the given method and instruction. */
  CallTarget callee(const Method* caller, const IRInstruction* instruction)
      const;

  /* Return a mapping from invoke instruction to artificial callees. */
  const std::unordered_map<const IRInstruction*, ArtificialCallees>&
  artificial_callees(const Method* caller) const;

  /* Return the artificial callees for an invoke instruction. */
  const ArtificialCallees& artificial_callees(
      const Method* caller,
      const IRInstruction* instruction) const;

  /* Return the field being accessed within the given method and instruction */
  const Field* MT_NULLABLE resolved_field_access(
      const Method* caller,
      const IRInstruction* instruction) const;

  Json::Value to_json(bool with_overrides = true) const;

 private:
  const Types& types_;
  const ClassHierarchies& class_hierarchies_;
  const Overrides& overrides_;

  ConcurrentMap<
      const Method*,
      std::unordered_map<const IRInstruction*, CallTarget>>
      resolved_base_callees_;
  ConcurrentMap<
      const Method*,
      std::unordered_map<const IRInstruction*, const Field*>>
      resolved_fields_;
  ConcurrentMap<
      const Method*,
      std::unordered_map<const IRInstruction*, ArtificialCallees>>
      artificial_callees_;
  std::unordered_map<const IRInstruction*, ArtificialCallees>
      empty_artificial_callees_map_;
  ArtificialCallees empty_artificial_callees_;
};

} // namespace marianatrench

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
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Issue.h>
#include <mariana-trench/LifecycleMethods.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/Types.h>
#include <mariana-trench/shim-generator/ShimGenerator.h>
#include <mariana-trench/shim-generator/Shims.h>

namespace marianatrench {

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
  /**
   * Call target for invoke-direct (a non-static direct method i.e.,
   * an instance method that is non-overridable (private or constructor))
   */
  static CallTarget direct_call(
      const IRInstruction* instruction,
      const Method* MT_NULLABLE callee,
      TextualOrderIndex call_index,
      const DexType* MT_NULLABLE receiver_type);

  /* Call target for invoke-static (direct calls without a receiver) */
  static CallTarget static_call(
      const IRInstruction* instruction,
      const Method* MT_NULLABLE callee,
      TextualOrderIndex call_index);

  static CallTarget virtual_call(
      const IRInstruction* instruction,
      const Method* MT_NULLABLE resolved_base_callee,
      TextualOrderIndex call_index,
      const DexType* MT_NULLABLE receiver_type,
      const std::unordered_set<const DexType*>* MT_NULLABLE
          receiver_local_extends,
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

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(CallTarget)

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
   * For a static and direct call, returns the callee.
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

  /* For a direct and virtual call, returns the inferred type of `this`. */
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
      const std::unordered_set<const DexType*>* MT_NULLABLE
          receiver_local_extends,
      const std::unordered_set<const DexType*>* MT_NULLABLE receiver_extends,
      const std::unordered_set<const Method*>* MT_NULLABLE overrides);

 private:
  const IRInstruction* instruction_;
  const Method* MT_NULLABLE resolved_base_callee_;
  TextualOrderIndex call_index_;
  const DexType* MT_NULLABLE receiver_type_;
  // Precise types that the receiver can be assigned at this callsite.
  const std::unordered_set<const DexType*>* MT_NULLABLE receiver_local_extends_;
  // All possible types that extend the receiver.
  const std::unordered_set<const DexType*>* MT_NULLABLE receiver_extends_;
  const std::unordered_set<const Method*>* MT_NULLABLE overrides_;
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
    boost::hash_combine(seed, call_target.receiver_local_extends_);
    boost::hash_combine(seed, call_target.receiver_extends_);
    boost::hash_combine(seed, call_target.overrides_);
    return seed;
  }
};

namespace marianatrench {

/**
 * Represents an artificial edge in the call graph.
 *
 * This is currently used to simulate calls to methods of anonymous classes
 * flowing through an external method and to simulate calls to shim targets.
 *
 * `root_registers` is the map of parameters from position to registers for
 * the artificial invoke instruction. If the root is not an argument position,
 * the instruction also accepts taint from other ports, typically call-effects,
 * which simulates flow of taint into the callee via means other than its
 * argument(s).
 */
struct ArtificialCallee {
  CallTarget call_target;
  std::unordered_map<Root, Register> root_registers;
  FeatureSet features;

  bool operator==(const ArtificialCallee& other) const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const ArtificialCallee& callee);
};

using ArtificialCallees = std::vector<ArtificialCallee>;

struct FieldTarget {
  const Field* field;
  TextualOrderIndex field_sink_index;

  bool operator==(const FieldTarget& other) const;

  friend std::ostream& operator<<(std::ostream& out, const FieldTarget& callee);
};

class CallGraph final {
 public:
  explicit CallGraph(
      const Options& options,
      const Types& types,
      const ClassHierarchies& class_hierarchies,
      const LifecycleMethods& lifecycle_methods,
      const Shims& shims,
      const FeatureFactory& feature_factory,
      Methods& methods,
      Fields& fields,
      Overrides& overrides,
      MethodMappings& method_mappings);

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(CallGraph)

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
  const std::optional<FieldTarget> resolved_field_access(
      const Method* caller,
      const IRInstruction* instruction) const;

  /* Used only for tests */
  const std::vector<FieldTarget> resolved_field_accesses(
      const Method* caller) const;

  /* Gets the index of the return instruction in the cfg of the method. Used for
   * issue handles */
  TextualOrderIndex return_index(
      const Method* caller,
      const IRInstruction* instruction) const;

  /* Used only for tests */
  const std::vector<TextualOrderIndex> return_indices(
      const Method* caller) const;

  /* Gets the index of the array allocation instruction in the cfg of the
   * method. Used for issue handles */
  TextualOrderIndex array_allocation_index(
      const Method* caller,
      const IRInstruction* instruction) const;

  /* Used only for tests */
  const std::vector<TextualOrderIndex> array_allocation_indices(
      const Method* caller) const;

  /* Returns whether the caller has any callees without constructing the
     intermediate structures. */
  bool has_callees(const Method* caller);

  Json::Value to_json(const Method* method, bool with_overrides = true) const;
  Json::Value to_json(bool with_overrides = true) const;

  void dump_call_graph(
      const std::filesystem::path& output_directory,
      bool with_overrides = true,
      const std::size_t batch_size =
          JsonValidation::k_default_shard_limit) const;

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
      std::unordered_map<const IRInstruction*, FieldTarget>>
      resolved_fields_;
  ConcurrentMap<
      const Method*,
      std::unordered_map<const IRInstruction*, ArtificialCallees>>
      artificial_callees_;
  std::unordered_map<const IRInstruction*, ArtificialCallees>
      empty_artificial_callees_map_;
  ArtificialCallees empty_artificial_callees_;
  ConcurrentMap<
      const Method*,
      std::unordered_map<const IRInstruction*, TextualOrderIndex>>
      indexed_returns_;
  ConcurrentMap<
      const Method*,
      std::unordered_map<const IRInstruction*, TextualOrderIndex>>
      indexed_array_allocations_;
};

} // namespace marianatrench

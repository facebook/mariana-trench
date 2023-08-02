/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sparta/AbstractDomain.h>
#include <sparta/ConstantAbstractDomain.h>

#include <DexClass.h>
#include <DexPosition.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/MemoryLocation.h>
#include <mariana-trench/MemoryLocationEnvironment.h>
#include <mariana-trench/SetterAccessPathConstantDomain.h>

namespace marianatrench {

using DexPositionDomain = sparta::ConstantAbstractDomain<DexPosition*>;

using LastParameterLoadDomain =
    sparta::ConstantAbstractDomain<ParameterPosition>;

// We cannot use `sparta::ReducedProductAbstractDomain` because it sets
// everything to bottom if a subdomain is bottom. Since the empty partition is
// considered bottom, this would always be bottom.
class ForwardAliasEnvironment final
    : public sparta::AbstractDomain<ForwardAliasEnvironment> {
 public:
  /* Create the bottom environment. */
  ForwardAliasEnvironment();

  ForwardAliasEnvironment(
      MemoryLocationEnvironment memory_locations,
      DexPositionDomain position,
      LastParameterLoadDomain last_parameter_load,
      SetterAccessPathConstantDomain field_write);

  /* Return the initial environment. */
  static ForwardAliasEnvironment initial();

  bool is_bottom() const;

  bool is_top() const;

  bool leq(const ForwardAliasEnvironment& other) const;

  bool equals(const ForwardAliasEnvironment& other) const;

  void set_to_bottom();

  void set_to_top();

  void join_with(const ForwardAliasEnvironment& other);

  void widen_with(const ForwardAliasEnvironment& other);

  void meet_with(const ForwardAliasEnvironment& other);

  void narrow_with(const ForwardAliasEnvironment& other);

  /* Set the memory location where the register points to. */
  void assign(Register register_id, MemoryLocation* memory_location);

  /* Set the memory locations where the register may point to. */
  void assign(Register register_id, MemoryLocationsDomain memory_locations);

  /* Return the memory locations where the register may point to. */
  MemoryLocationsDomain memory_locations(Register register_id) const;

  /* Return the memory locations for the given field of the given register. */
  MemoryLocationsDomain memory_locations(
      Register register_id,
      const DexString* field) const;

  const MemoryLocationEnvironment& memory_location_environment() const;

  DexPosition* MT_NULLABLE last_position() const;
  void set_last_position(DexPosition* position);
  const LastParameterLoadDomain& last_parameter_loaded() const;

  const SetterAccessPathConstantDomain& field_write() const;
  void set_field_write(SetterAccessPathConstantDomain field_write);

  void increment_last_parameter_loaded();

  friend std::ostream& operator<<(
      std::ostream& out,
      const ForwardAliasEnvironment& environment);

 private:
  MemoryLocationEnvironment memory_locations_;
  DexPositionDomain position_;
  LastParameterLoadDomain last_parameter_load_;

  // Used to infer a trivial setter.
  // * This is bottom if no iput instruction was seen
  // * This is top if an iput was seen but is not trivial
  // * This is top if multiple iput instructions were seen
  SetterAccessPathConstantDomain field_write_;
};

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <AbstractDomain.h>
#include <ConstantAbstractDomain.h>
#include <PatriciaTreeMapAbstractPartition.h>
#include <PatriciaTreeSetAbstractDomain.h>

#include <DexClass.h>
#include <DexPosition.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/MemoryLocation.h>
#include <mariana-trench/MemoryLocationEnvironment.h>
#include <mariana-trench/Taint.h>
#include <mariana-trench/TaintEnvironment.h>
#include <mariana-trench/TaintTree.h>

namespace marianatrench {

using DexPositionDomain = sparta::ConstantAbstractDomain<DexPosition*>;

using LastParameterLoadDomain =
    sparta::ConstantAbstractDomain<ParameterPosition>;

// We cannot use `sparta::ReducedProductAbstractDomain` because it sets
// everything to bottom if a subdomain is bottom. Since the empty partition is
// considered bottom, this would always be bottom.
class ForwardTaintEnvironment final
    : public sparta::AbstractDomain<ForwardTaintEnvironment> {
 public:
  /* Create the bottom environment. */
  ForwardTaintEnvironment();

  ForwardTaintEnvironment(
      MemoryLocationEnvironment memory_locations,
      TaintEnvironment taint,
      DexPositionDomain position,
      LastParameterLoadDomain last_parameter_load);

  /* Return the initial environment. */
  static ForwardTaintEnvironment initial();

  bool is_bottom() const override;

  bool is_top() const override;

  bool leq(const ForwardTaintEnvironment& other) const override;

  bool equals(const ForwardTaintEnvironment& other) const override;

  void set_to_bottom() override;

  void set_to_top() override;

  void join_with(const ForwardTaintEnvironment& other) override;

  void widen_with(const ForwardTaintEnvironment& other) override;

  void meet_with(const ForwardTaintEnvironment& other) override;

  void narrow_with(const ForwardTaintEnvironment& other) override;

  const MemoryLocationEnvironment& memory_location_environment() const;

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

  TaintTree read(MemoryLocation* memory_location) const;

  TaintTree read(MemoryLocation* memory_location, const Path& path) const;

  TaintTree read(const MemoryLocationsDomain& memory_locations) const;

  TaintTree read(Register register_id) const;

  TaintTree read(Register register_id, const Path& path) const;

  void write(MemoryLocation* memory_location, TaintTree taint, UpdateKind kind);

  void write(
      MemoryLocation* memory_location,
      const Path& path,
      TaintTree taint,
      UpdateKind kind);

  void write(
      MemoryLocation* memory_location,
      const Path& path,
      Taint taint,
      UpdateKind kind);

  void write(Register register_id, TaintTree taint, UpdateKind kind);

  void write(
      Register register_id,
      const Path& path,
      TaintTree taint,
      UpdateKind kind);

  void
  write(Register register_id, const Path& path, Taint taint, UpdateKind kind);

  DexPosition* MT_NULLABLE last_position() const;

  void set_last_position(DexPosition* position);

  const LastParameterLoadDomain& last_parameter_loaded() const;

  void increment_last_parameter_loaded();

  friend std::ostream& operator<<(
      std::ostream& out,
      const ForwardTaintEnvironment& environment);

 private:
  MemoryLocationEnvironment memory_locations_;
  TaintEnvironment taint_;
  DexPositionDomain position_;
  LastParameterLoadDomain last_parameter_load_;
};

} // namespace marianatrench

/*
 * Copyright (c) Facebook, Inc. and its affiliates.
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

#include <mariana-trench/Access.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/MemoryLocation.h>
#include <mariana-trench/Taint.h>
#include <mariana-trench/TaintTree.h>

namespace marianatrench {

using MemoryLocationsDomain =
    sparta::PatriciaTreeSetAbstractDomain<MemoryLocation*>;
using MemoryLocationsPartition =
    sparta::PatriciaTreeMapAbstractPartition<Register, MemoryLocationsDomain>;

using TaintAbstractPartition =
    sparta::PatriciaTreeMapAbstractPartition<MemoryLocation*, TaintTree>;

using DexPositionDomain = sparta::ConstantAbstractDomain<DexPosition*>;

using LastParameterLoadDomain =
    sparta::ConstantAbstractDomain<ParameterPosition>;

// We cannot use `sparta::ReducedProductAbstractDomain` because it sets
// everything to bottom if a subdomain is bottom. Since the empty partition is
// considered bottom, this would always be bottom.
class AnalysisEnvironment final
    : public sparta::AbstractDomain<AnalysisEnvironment> {
 public:
  /* Create the bottom environment. */
  AnalysisEnvironment();

  AnalysisEnvironment(
      MemoryLocationsPartition memory_locations,
      TaintAbstractPartition taint,
      DexPositionDomain position,
      LastParameterLoadDomain last_parameter_load);

  /* Return the initial environment. */
  static AnalysisEnvironment initial();

  bool is_bottom() const override;

  bool is_top() const override;

  bool leq(const AnalysisEnvironment& other) const override;

  bool equals(const AnalysisEnvironment& other) const override;

  void set_to_bottom() override;

  void set_to_top() override;

  void join_with(const AnalysisEnvironment& other) override;

  void widen_with(const AnalysisEnvironment& other) override;

  void meet_with(const AnalysisEnvironment& other) override;

  void narrow_with(const AnalysisEnvironment& other) override;

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
      const AnalysisEnvironment& environment);

 private:
  MemoryLocationsPartition memory_locations_;
  TaintAbstractPartition taint_;
  DexPositionDomain position_;
  LastParameterLoadDomain last_parameter_load_;
};

std::ostream& operator<<(
    std::ostream& out,
    const marianatrench::MemoryLocationsPartition& memory_locations);

std::ostream& operator<<(
    std::ostream& out,
    const marianatrench::TaintAbstractPartition& taint);

} // namespace marianatrench

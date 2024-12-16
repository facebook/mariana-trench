/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>
#include <unordered_map>

#include <IRInstruction.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/MemoryLocation.h>
#include <mariana-trench/MemoryLocationEnvironment.h>
#include <mariana-trench/PointsToEnvironment.h>

namespace marianatrench {

/* Alias information about a specific instruction. */
class InstructionAliasResults final {
 public:
  InstructionAliasResults(
      RegisterMemoryLocationsMap register_memory_locations_map,
      ResolvedAliasesMap aliases,
      std::optional<MemoryLocationsDomain> result_memory_locations,
      DexPosition* MT_NULLABLE position);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(InstructionAliasResults)

  /* Mapping from registers to their memory locations *before* the instruction
   * (precondition). */
  const RegisterMemoryLocationsMap& register_memory_locations_map() const;

  /* Mapping from root memory locations to the corresponding resolved points-to
   * tree after analyzing the instruction */
  const ResolvedAliasesMap& resolved_aliases() const;

  /* Memory locations pointed by the given register *before* the instruction. */
  MemoryLocationsDomain register_memory_locations(Register register_id) const;

  /* Memory locations pointed by the destination register of the instruction,
   * if any. */
  MemoryLocationsDomain result_memory_locations() const;

  /* Result memory location of the instruction, if the instruction has a
   * destination register and its memory location is a singleton. */
  MemoryLocation* result_memory_location() const;

  /* Result memory location of the instruction, or nullptr. */
  MemoryLocation* MT_NULLABLE result_memory_location_or_null() const;

  /* Position of the instruction, or nullptr. */
  DexPosition* MT_NULLABLE position() const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const InstructionAliasResults& instruction_alias_results);

 private:
  RegisterMemoryLocationsMap register_memory_locations_map_;
  ResolvedAliasesMap aliases_;
  std::optional<MemoryLocationsDomain> result_memory_locations_;
  DexPosition* MT_NULLABLE position_;
};

/**
 * Represents the result of the forward alias analysis.
 * This is passed to the forward and backward taint analysis.
 */
class AliasAnalysisResults final {
 public:
  AliasAnalysisResults() = default;

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(AliasAnalysisResults)

  const InstructionAliasResults& get(const IRInstruction* instruction) const;

  void store(const IRInstruction* instruction, InstructionAliasResults results);

 private:
  std::unordered_map<const IRInstruction*, InstructionAliasResults>
      instructions_;
};

} // namespace marianatrench

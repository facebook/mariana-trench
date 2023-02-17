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
#include <mariana-trench/MemoryLocation.h>
#include <mariana-trench/MemoryLocationEnvironment.h>

namespace marianatrench {

/* Alias information about a specific instruction. */
class InstructionAliasResults final {
 public:
  InstructionAliasResults(
      MemoryLocationEnvironment memory_location_environment,
      std::optional<MemoryLocationsDomain> result_memory_locations,
      DexPosition* MT_NULLABLE position);

  InstructionAliasResults(const InstructionAliasResults&) = default;
  InstructionAliasResults(InstructionAliasResults&&) = default;
  InstructionAliasResults& operator=(const InstructionAliasResults&) = default;
  InstructionAliasResults& operator=(InstructionAliasResults&&) = default;
  ~InstructionAliasResults() = default;

  /* Memory location environment *before* the instruction (precondition). */
  const MemoryLocationEnvironment& memory_location_environment() const;

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

 private:
  MemoryLocationEnvironment memory_location_environment_;
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
  AliasAnalysisResults(const AliasAnalysisResults&) = delete;
  AliasAnalysisResults(AliasAnalysisResults&&) = delete;
  AliasAnalysisResults& operator=(const AliasAnalysisResults&) = delete;
  AliasAnalysisResults& operator=(AliasAnalysisResults&&) = delete;
  ~AliasAnalysisResults() = default;

  const InstructionAliasResults& get(const IRInstruction* instruction) const;

  void store(const IRInstruction* instruction, InstructionAliasResults results);

 private:
  std::unordered_map<const IRInstruction*, InstructionAliasResults>
      instructions_;
};

} // namespace marianatrench

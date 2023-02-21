/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>

#include <Show.h>

#include <mariana-trench/AliasAnalysisResults.h>
#include <mariana-trench/Assert.h>

namespace marianatrench {

InstructionAliasResults::InstructionAliasResults(
    MemoryLocationEnvironment memory_location_environment,
    std::optional<MemoryLocationsDomain> result_memory_locations,
    DexPosition* MT_NULLABLE position)
    : memory_location_environment_(std::move(memory_location_environment)),
      result_memory_locations_(std::move(result_memory_locations)),
      position_(position) {}

const MemoryLocationEnvironment&
InstructionAliasResults::memory_location_environment() const {
  return memory_location_environment_;
}

MemoryLocationsDomain InstructionAliasResults::register_memory_locations(
    Register register_id) const {
  return memory_location_environment_.get(register_id);
}

MemoryLocationsDomain InstructionAliasResults::result_memory_locations() const {
  mt_assert(result_memory_locations_.has_value());
  return *result_memory_locations_;
}

MemoryLocation* InstructionAliasResults::result_memory_location() const {
  mt_assert(result_memory_locations_.has_value());
  const auto& memory_locations = *result_memory_locations_;
  mt_assert(memory_locations.size() == 1);
  return *memory_locations.elements().begin();
}

MemoryLocation* MT_NULLABLE
InstructionAliasResults::result_memory_location_or_null() const {
  if (!result_memory_locations_.has_value()) {
    return nullptr;
  }

  const auto& memory_locations = *result_memory_locations_;
  if (memory_locations.size() != 1) {
    return nullptr;
  }

  return *memory_locations.elements().begin();
}

DexPosition* MT_NULLABLE InstructionAliasResults::position() const {
  return position_;
}

const InstructionAliasResults& AliasAnalysisResults::get(
    const IRInstruction* instruction) const {
  mt_assert(instruction != nullptr);

  auto it = instructions_.find(instruction);
  if (it == instructions_.end()) {
    // We might not have saved alias information for that instruction,
    // see `ShouldStoreAliasResults` in ForwardAliasFixpoint.cpp
    throw std::runtime_error(fmt::format(
        "No alias information for instruction `{}`", show(instruction)));
  }

  return it->second;
}

void AliasAnalysisResults::store(
    const IRInstruction* instruction,
    InstructionAliasResults results) {
  instructions_.insert_or_assign(instruction, std::move(results));
}

} // namespace marianatrench

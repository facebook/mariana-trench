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
#include <mariana-trench/Log.h>

namespace marianatrench {

InstructionAliasResults::InstructionAliasResults(
    RegisterMemoryLocationsMap register_memory_locations_map,
    WideningPointsToResolver widening_resolver,
    std::optional<MemoryLocationsDomain> result_memory_locations,
    DexPosition* MT_NULLABLE position)
    : register_memory_locations_map_(std::move(register_memory_locations_map)),
      widening_resolver_(std::move(widening_resolver)),
      result_memory_locations_(std::move(result_memory_locations)),
      position_(position) {}

const RegisterMemoryLocationsMap&
InstructionAliasResults::register_memory_locations_map() const {
  return register_memory_locations_map_;
}

const WideningPointsToResolver& InstructionAliasResults::widening_resolver()
    const {
  return widening_resolver_;
}

MemoryLocationsDomain InstructionAliasResults::register_memory_locations(
    Register register_id) const {
  return register_memory_locations_map_.at(register_id);
}

MemoryLocationsDomain InstructionAliasResults::result_memory_locations() const {
  mt_assert(result_memory_locations_.has_value());
  return *result_memory_locations_;
}

MemoryLocation* InstructionAliasResults::result_memory_location() const {
  mt_assert(result_memory_locations_.has_value());
  const auto& memory_locations = *result_memory_locations_;
  auto* singleton = memory_locations.singleton();
  mt_assert(singleton != nullptr);
  return *singleton;
}

MemoryLocation* MT_NULLABLE
InstructionAliasResults::result_memory_location_or_null() const {
  if (!result_memory_locations_.has_value()) {
    return nullptr;
  }

  const auto& memory_locations = *result_memory_locations_;
  auto* singleton = memory_locations.singleton();
  if (singleton == nullptr) {
    return nullptr;
  }

  return *singleton;
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

std::ostream& operator<<(
    std::ostream& out,
    const InstructionAliasResults& results) {
  out << "InstructionAliasResults(\nregister_memory_locations_map={\n";
  for (const auto& [register_id, memory_locations] :
       results.register_memory_locations_map_) {
    out << fmt::format("{} -> {},\n", register_id, memory_locations);
  }

  out << "},\nwidening_resolver=" << results.widening_resolver_;

  out << "},\nresult_memory_locations="
      << show(results.result_memory_location_or_null());

  out << ",\nposition=" << show(results.position());

  return out << ")";
}

void AliasAnalysisResults::store(
    const IRInstruction* instruction,
    InstructionAliasResults results) {
  LOG(5,
      "Storing instruction alias results for `{}`: {}",
      show(instruction),
      results);
  instructions_.insert_or_assign(instruction, std::move(results));
}

} // namespace marianatrench

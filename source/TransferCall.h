/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <limits>

#include <mariana-trench/Access.h>
#include <mariana-trench/CallGraph.h>
#include <mariana-trench/MemoryLocation.h>
#include <mariana-trench/MemoryLocationEnvironment.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/MethodContext.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/Position.h>
#include <mariana-trench/TaintTree.h>

namespace marianatrench {

namespace {
constexpr Register k_result_register = std::numeric_limits<Register>::max();
}

void log_instruction(
    const MethodContext* context,
    const IRInstruction* instruction);

struct CalleeModel {
  const DexMethodRef* method_reference;
  const Method* MT_NULLABLE resolved_base_method;
  const Position* position;
  TextualOrderIndex call_index;
  Model model;
};

std::vector<const DexType * MT_NULLABLE> get_source_register_types(
    const MethodContext* context,
    const IRInstruction* instruction);

std::vector<std::optional<std::string>> get_source_constant_arguments(
    const RegisterMemoryLocationsMap& register_memory_locations_map,
    const IRInstruction* instruction);

bool get_is_this_call(
    const RegisterMemoryLocationsMap& register_memory_locations_map,
    const IRInstruction* instruction);

CalleeModel get_callee(
    const MethodContext* context,
    const IRInstruction* instruction,
    const DexPosition* MT_NULLABLE position,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments,
    bool is_this_call);

CalleeModel get_callee(
    const MethodContext* context,
    const ArtificialCallee& callee,
    const DexPosition* MT_NULLABLE dex_position);

/* If the method invoke can be safely inlined as a getter, return the result
 * memory location, otherwise return nullptr. */
MemoryLocation* MT_NULLABLE try_inline_invoke_as_getter(
    const MethodContext* context,
    const RegisterMemoryLocationsMap& register_memory_locations_map,
    const IRInstruction* instruction,
    const CalleeModel& callee);

struct SetterInlineMemoryLocations {
  MemoryLocation* target;
  MemoryLocation* value;
  const Position* position;
};

/* If the method invoke can be safely inlined as a setter, return the target
 * and value memory locations, otherwise return nullopt. */
std::optional<SetterInlineMemoryLocations> try_inline_invoke_as_setter(
    const MethodContext* context,
    const RegisterMemoryLocationsMap& register_memory_locations_map,
    const IRInstruction* instruction,
    const CalleeModel& callee);

/* Add a set of hardcoded features on field access. */
void add_field_features(
    MethodContext* context,
    TaintTree& taint_tree,
    const FieldMemoryLocation* field_memory_location);

/* Get the locally inferred feature to add to the aliasing memory location */
FeatureMayAlwaysSet get_field_features(
    MethodContext* context,
    const FieldMemoryLocation* field_memory_location);

} // namespace marianatrench

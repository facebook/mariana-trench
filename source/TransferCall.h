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
#include <mariana-trench/MemoryLocationEnvironment.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/MethodContext.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/Position.h>

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
    const MemoryLocationEnvironment& memory_location_environment,
    const IRInstruction* instruction);

CalleeModel get_callee(
    const MethodContext* context,
    const IRInstruction* instruction,
    const DexPosition* MT_NULLABLE position,
    const std::vector<const DexType * MT_NULLABLE>& source_register_types,
    const std::vector<std::optional<std::string>>& source_constant_arguments);

CalleeModel get_callee(
    const MethodContext* context,
    const ArtificialCallee& callee,
    const DexPosition* MT_NULLABLE dex_position);

/* If the method invoke can be safely inlined, return the result memory
 * location, otherwise return nullptr. */
MemoryLocation* MT_NULLABLE try_inline_invoke(
    const MethodContext* context,
    const MemoryLocationEnvironment& memory_location_environment,
    const IRInstruction* instruction,
    const CalleeModel& callee);

} // namespace marianatrench

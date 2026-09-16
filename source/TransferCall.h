/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

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

void log_instruction(
    const MethodContext* context,
    const IRInstruction* instruction);

/**
 * The constant value of a `static final` field read by an `sget*`, or
 * `std::nullopt`.
 *
 * `InstructionMemoryLocation::get_constant()` only answers for instructions
 * that carry a literal or a string operand, so an argument loaded from a
 * constant static field resolves to nothing and a `via_value_of` model on it
 * degrades to `unknown`. Java source rarely produces this shape, because javac
 * folds reads of compile-time constants into the use site, but generated
 * bytecode routinely does.
 *
 * Only `final` fields are read. A mutable static can be written after
 * `<clinit>`, so its encoded value is not what the read observes.
 */
[[nodiscard]] std::optional<std::string> static_final_field_constant(
    const IRInstruction* instruction);

/**
 * An identity for the object held in a `static final` field read by an
 * `sget-object`, as `Lcom/example/Holder;.field_name`, or `std::nullopt`.
 *
 * A reference has no constant value, so `via_value_of` on an object argument
 * can only ever say `unknown`. Naming the field it came from is the most
 * specific true thing available, and for a generated constant-holder class it
 * is a stable identity: the field is `final`, so it denotes the same object on
 * every run, and unlike the object's contents the name does not change between
 * builds.
 *
 * Unlike `static_final_field_constant` this needs no `<clinit>` guard. It makes
 * no claim about the object's contents, only about which field holds it, and
 * `final` already guarantees that binding is written once.
 */
[[nodiscard]] std::optional<std::string> static_final_field_identity(
    const IRInstruction* instruction);

struct CalleeModel {
  const DexMethodRef* method_reference;
  const Method* MT_NULLABLE resolved_base_method;
  const Position* position;
  CallTarget::CallKind call_kind;
  TextualOrderIndex call_index;
  // Resolved values of constant arguments passed to the callee.
  const std::vector<std::optional<std::string>> source_constant_arguments;
  Model model;
};

CalleeModel get_callee(
    const MethodContext* context,
    const IRInstruction* instruction,
    const DexPosition* MT_NULLABLE dex_position,
    const RegisterMemoryLocationsMap& register_memory_locations_map);

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
    const CalleeModel& callee,
    const ArtificialCallees& callees);

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
    const CalleeModel& callee,
    const ArtificialCallees& callees);

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

/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>

#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/MemoryLocation.h>

namespace marianatrench {

FieldMemoryLocation* MemoryLocation::make_field(const DexString* field) {
  mt_assert(field != nullptr);

  auto found = fields_.find(field);
  if (found != fields_.end()) {
    return found->second.get();
  }

  // To avoid non-convergence, we need to break infinite chains.
  // If any parent of `this` is already a `FieldMemoryLocation` for the given
  // field, return it in order to collapse the memory locations into a single
  // location.
  MemoryLocation* memory_location = this;
  while (auto* field_memory_location =
             memory_location->dyn_cast<FieldMemoryLocation>()) {
    if (field_memory_location->field() == field) {
      return field_memory_location;
    } else {
      memory_location = field_memory_location->parent();
    }
  }

  auto result = fields_.emplace(
      field, std::make_unique<FieldMemoryLocation>(this, field));
  mt_assert(result.second);
  return result.first->second.get();
}

MemoryLocation* MemoryLocation::root() {
  // By defaut, this is a root memory location.
  return this;
}

const Path& MemoryLocation::path() {
  // By default, this is a root memory location.
  static const Path empty_path;
  return empty_path;
}

std::optional<AccessPath> MemoryLocation::access_path() {
  auto* parameter = root()->dyn_cast<ParameterMemoryLocation>();
  if (!parameter) {
    return std::nullopt;
  }

  return AccessPath(Root(Root::Kind::Argument, parameter->position()), path());
}

std::ostream& operator<<(
    std::ostream& out,
    const MemoryLocation& memory_location) {
  return out << memory_location.str();
}

ParameterMemoryLocation::ParameterMemoryLocation(ParameterPosition position)
    : position_(position) {}

std::string ParameterMemoryLocation::str() const {
  return fmt::format("ParameterMemoryLocation({})", position_);
}

ThisParameterMemoryLocation::ThisParameterMemoryLocation()
    : ParameterMemoryLocation(0) {}

std::string ThisParameterMemoryLocation::str() const {
  return "ThisParameterMemoryLocation";
}

FieldMemoryLocation::FieldMemoryLocation(
    MemoryLocation* parent,
    const DexString* field)
    : parent_(parent),
      field_(field),
      root_(parent->root()),
      path_(parent->path()) {
  mt_assert(parent != nullptr);
  mt_assert(field != nullptr);

  path_.append(field);
}

std::string FieldMemoryLocation::str() const {
  return fmt::format(
      "FieldMemoryLocation({}, `{}`)", parent_->str(), show(field_));
}

MemoryLocation* FieldMemoryLocation::root() {
  return root_;
}

const Path& FieldMemoryLocation::path() {
  return path_;
}

InstructionMemoryLocation::InstructionMemoryLocation(
    const IRInstruction* instruction)
    : instruction_(instruction) {
  mt_assert(instruction != nullptr);
}

std::string InstructionMemoryLocation::str() const {
  return fmt::format("InstructionMemoryLocation(`{}`)", show(instruction_));
}

MemoryFactory::MemoryFactory(const Method* method) {
  mt_assert(method != nullptr);

  // Create parameter locations
  for (ParameterPosition i = 0; i < method->number_of_parameters(); i++) {
    if (i == 0 && !method->is_static()) {
      parameters_.push_back(std::make_unique<ThisParameterMemoryLocation>());
    } else {
      parameters_.push_back(std::make_unique<ParameterMemoryLocation>(i));
    }
  }
}

ParameterMemoryLocation* MemoryFactory::make_parameter(
    ParameterPosition parameter_position) {
  if (parameter_position < 0u || parameter_position >= parameters_.size()) {
    throw std::out_of_range(
        "Accessing out of bounds parameter in memory factory.");
  }

  return parameters_[parameter_position].get();
}

InstructionMemoryLocation* MemoryFactory::make_location(
    const IRInstruction* instruction) {
  mt_assert(instruction != nullptr);

  auto found = instructions_.find(instruction);
  if (found != instructions_.end()) {
    return found->second.get();
  }

  auto result = instructions_.emplace(
      instruction, std::make_unique<InstructionMemoryLocation>(instruction));
  mt_assert(result.second);
  return result.first->second.get();
}

} // namespace marianatrench

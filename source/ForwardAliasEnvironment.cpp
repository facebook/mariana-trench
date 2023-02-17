/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/ForwardAliasEnvironment.h>

namespace marianatrench {

ForwardAliasEnvironment::ForwardAliasEnvironment()
    : memory_locations_(MemoryLocationEnvironment::bottom()),
      position_(DexPositionDomain::bottom()),
      last_parameter_load_(LastParameterLoadDomain::bottom()) {}

ForwardAliasEnvironment::ForwardAliasEnvironment(
    MemoryLocationEnvironment memory_locations,
    DexPositionDomain position,
    LastParameterLoadDomain last_parameter_load)
    : memory_locations_(std::move(memory_locations)),
      position_(std::move(position)),
      last_parameter_load_(std::move(last_parameter_load)) {}

ForwardAliasEnvironment ForwardAliasEnvironment::initial() {
  return ForwardAliasEnvironment(
      MemoryLocationEnvironment::bottom(),
      DexPositionDomain::top(),
      LastParameterLoadDomain(0));
}

bool ForwardAliasEnvironment::is_bottom() const {
  return memory_locations_.is_bottom() && position_.is_bottom() &&
      last_parameter_load_.is_bottom();
}

bool ForwardAliasEnvironment::is_top() const {
  return memory_locations_.is_top() && position_.is_top() &&
      last_parameter_load_.is_top();
}

bool ForwardAliasEnvironment::leq(const ForwardAliasEnvironment& other) const {
  return memory_locations_.leq(other.memory_locations_) &&
      position_.leq(other.position_) &&
      last_parameter_load_.leq(other.last_parameter_load_);
}

bool ForwardAliasEnvironment::equals(
    const ForwardAliasEnvironment& other) const {
  return memory_locations_.equals(other.memory_locations_) &&
      position_.equals(other.position_) &&
      last_parameter_load_.equals(other.last_parameter_load_);
}

void ForwardAliasEnvironment::set_to_bottom() {
  memory_locations_.set_to_bottom();
  position_.set_to_bottom();
  last_parameter_load_.set_to_bottom();
}

void ForwardAliasEnvironment::set_to_top() {
  memory_locations_.set_to_top();
  position_.set_to_top();
  last_parameter_load_.set_to_top();
}

void ForwardAliasEnvironment::join_with(const ForwardAliasEnvironment& other) {
  mt_if_expensive_assert(auto previous = *this);

  memory_locations_.join_with(other.memory_locations_);
  position_.join_with(other.position_);
  last_parameter_load_.join_with(other.last_parameter_load_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void ForwardAliasEnvironment::widen_with(const ForwardAliasEnvironment& other) {
  mt_if_expensive_assert(auto previous = *this);

  memory_locations_.widen_with(other.memory_locations_);
  position_.widen_with(other.position_);
  last_parameter_load_.widen_with(other.last_parameter_load_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void ForwardAliasEnvironment::meet_with(const ForwardAliasEnvironment& other) {
  memory_locations_.meet_with(other.memory_locations_);
  position_.meet_with(other.position_);
  last_parameter_load_.meet_with(other.last_parameter_load_);
}

void ForwardAliasEnvironment::narrow_with(
    const ForwardAliasEnvironment& other) {
  memory_locations_.narrow_with(other.memory_locations_);
  position_.narrow_with(other.position_);
  last_parameter_load_.narrow_with(other.last_parameter_load_);
}

void ForwardAliasEnvironment::assign(
    Register register_id,
    MemoryLocation* memory_location) {
  memory_locations_.set(register_id, MemoryLocationsDomain{memory_location});
}

void ForwardAliasEnvironment::assign(
    Register register_id,
    MemoryLocationsDomain memory_locations) {
  mt_assert(!memory_locations.is_top());

  memory_locations_.set(register_id, memory_locations);
}

MemoryLocationsDomain ForwardAliasEnvironment::memory_locations(
    Register register_id) const {
  MemoryLocationsDomain memory_locations = memory_locations_.get(register_id);

  if (!memory_locations.is_value()) {
    // Return an empty set instead of top or bottom.
    memory_locations = {};
  }

  return memory_locations;
}

MemoryLocationsDomain ForwardAliasEnvironment::memory_locations(
    Register register_id,
    const DexString* field) const {
  MemoryLocationsDomain memory_locations = memory_locations_.get(register_id);

  if (!memory_locations.is_value()) {
    memory_locations = {};
  }

  MemoryLocationsDomain fields;
  for (auto* memory_location : memory_locations.elements()) {
    fields.add(memory_location->make_field(field));
  }
  return fields;
}

const MemoryLocationEnvironment&
ForwardAliasEnvironment::memory_location_environment() const {
  return memory_locations_;
}

DexPosition* ForwardAliasEnvironment::last_position() const {
  return get_optional_value_or(position_.get_constant(), nullptr);
}

void ForwardAliasEnvironment::set_last_position(DexPosition* position) {
  position_ = DexPositionDomain(position);
}

const LastParameterLoadDomain& ForwardAliasEnvironment::last_parameter_loaded()
    const {
  return last_parameter_load_;
}

void ForwardAliasEnvironment::increment_last_parameter_loaded() {
  if (last_parameter_load_.is_value()) {
    last_parameter_load_ =
        LastParameterLoadDomain(*last_parameter_load_.get_constant() + 1);
  }
}

std::ostream& operator<<(
    std::ostream& out,
    const ForwardAliasEnvironment& environment) {
  return out << "(memory_locations=" << environment.memory_locations_
             << ", position=" << environment.position_
             << ", last_parameter_load=" << environment.last_parameter_load_
             << ")";
}

} // namespace marianatrench

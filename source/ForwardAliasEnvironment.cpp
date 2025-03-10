/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/ForwardAliasEnvironment.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/WideningPointsToResolver.h>

namespace marianatrench {

ForwardAliasEnvironment::ForwardAliasEnvironment()
    : memory_locations_(MemoryLocationEnvironment::bottom()),
      aliases_(PointsToEnvironment::bottom()),
      position_(DexPositionDomain::bottom()),
      last_parameter_load_(LastParameterLoadDomain::bottom()),
      field_write_(SetterAccessPathConstantDomain::bottom()) {}

ForwardAliasEnvironment::ForwardAliasEnvironment(
    MemoryLocationEnvironment memory_locations,
    PointsToEnvironment aliases,
    DexPositionDomain position,
    LastParameterLoadDomain last_parameter_load,
    SetterAccessPathConstantDomain field_write)
    : memory_locations_(std::move(memory_locations)),
      aliases_(std::move(aliases)),
      position_(std::move(position)),
      last_parameter_load_(std::move(last_parameter_load)),
      field_write_(field_write) {}

ForwardAliasEnvironment ForwardAliasEnvironment::initial() {
  return ForwardAliasEnvironment(
      MemoryLocationEnvironment::bottom(),
      PointsToEnvironment::bottom(),
      DexPositionDomain::top(),
      LastParameterLoadDomain(0),
      SetterAccessPathConstantDomain::bottom());
}

bool ForwardAliasEnvironment::is_bottom() const {
  return memory_locations_.is_bottom() && aliases_.is_bottom() &&
      position_.is_bottom() && last_parameter_load_.is_bottom() &&
      field_write_.is_bottom();
}

bool ForwardAliasEnvironment::is_top() const {
  return memory_locations_.is_top() && aliases_.is_top() &&
      position_.is_top() && last_parameter_load_.is_top() &&
      field_write_.is_top();
}

bool ForwardAliasEnvironment::leq(const ForwardAliasEnvironment& other) const {
  return memory_locations_.leq(other.memory_locations_) &&
      aliases_.leq(other.aliases_) && position_.leq(other.position_) &&
      last_parameter_load_.leq(other.last_parameter_load_) &&
      field_write_.leq(other.field_write_);
}

bool ForwardAliasEnvironment::equals(
    const ForwardAliasEnvironment& other) const {
  return memory_locations_.equals(other.memory_locations_) &&
      aliases_.equals(other.aliases_) && position_.equals(other.position_) &&
      last_parameter_load_.equals(other.last_parameter_load_) &&
      field_write_.equals(other.field_write_);
}

void ForwardAliasEnvironment::set_to_bottom() {
  memory_locations_.set_to_bottom();
  aliases_.set_to_bottom();
  position_.set_to_bottom();
  last_parameter_load_.set_to_bottom();
  field_write_.set_to_bottom();
}

void ForwardAliasEnvironment::set_to_top() {
  memory_locations_.set_to_top();
  aliases_.set_to_top();
  position_.set_to_top();
  last_parameter_load_.set_to_top();
  field_write_.set_to_top();
}

void ForwardAliasEnvironment::join_with(const ForwardAliasEnvironment& other) {
  mt_if_expensive_assert(auto previous = *this);

  memory_locations_.join_with(other.memory_locations_);
  aliases_.join_with(other.aliases_);
  position_.join_with(other.position_);
  last_parameter_load_.join_with(other.last_parameter_load_);
  field_write_.join_with(other.field_write_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void ForwardAliasEnvironment::widen_with(const ForwardAliasEnvironment& other) {
  mt_if_expensive_assert(auto previous = *this);

  memory_locations_.widen_with(other.memory_locations_);
  aliases_.widen_with(other.aliases_);
  position_.widen_with(other.position_);
  last_parameter_load_.widen_with(other.last_parameter_load_);
  field_write_.widen_with(other.field_write_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void ForwardAliasEnvironment::meet_with(const ForwardAliasEnvironment& other) {
  memory_locations_.meet_with(other.memory_locations_);
  aliases_.meet_with(other.aliases_);
  position_.meet_with(other.position_);
  last_parameter_load_.meet_with(other.last_parameter_load_);
  field_write_.meet_with(other.field_write_);
}

void ForwardAliasEnvironment::narrow_with(
    const ForwardAliasEnvironment& other) {
  memory_locations_.narrow_with(other.memory_locations_);
  aliases_.narrow_with(other.aliases_);
  position_.narrow_with(other.position_);
  last_parameter_load_.narrow_with(other.last_parameter_load_);
  field_write_.narrow_with(other.field_write_);
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
  return memory_locations_.get(register_id);
}

MemoryLocationsDomain ForwardAliasEnvironment::memory_locations(
    Register register_id,
    const DexString* field) const {
  MemoryLocationsDomain memory_locations = memory_locations_.get(register_id);

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

WideningPointsToResolver ForwardAliasEnvironment::make_widening_resolver()
    const {
  return aliases_.make_widening_resolver();
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

const SetterAccessPathConstantDomain& ForwardAliasEnvironment::field_write()
    const {
  return field_write_;
}

void ForwardAliasEnvironment::set_field_write(
    SetterAccessPathConstantDomain field_write) {
  field_write_ = std::move(field_write);
}

PointsToSet ForwardAliasEnvironment::points_to(
    MemoryLocation* memory_location) const {
  auto points_tos = aliases_.points_to(memory_location);

  LOG(5,
      "Resolved points-to for memory location {} to {}",
      show(memory_location),
      points_tos);

  return points_tos;
}

PointsToSet ForwardAliasEnvironment::points_to(
    const MemoryLocationsDomain& memory_locations) const {
  auto points_tos = aliases_.points_to(memory_locations);

  LOG(5,
      "Resolved points-to for memory locations {} to {}",
      memory_locations,
      points_tos);

  return points_tos;
}

void ForwardAliasEnvironment::write(
    const WideningPointsToResolver& widening_resolver,
    MemoryLocation* memory_location,
    const DexString* field,
    const PointsToSet& points_tos,
    UpdateKind kind) {
  LOG(5,
      "{} update points-to tree at: {} field `{}` with {}",
      kind == UpdateKind::Strong ? "Strong" : "Weak",
      show(memory_location),
      field->str(),
      points_tos);

  aliases_.write(widening_resolver, memory_location, field, points_tos, kind);
}

std::ostream& operator<<(
    std::ostream& out,
    const ForwardAliasEnvironment& environment) {
  return out << "(memory_locations=" << environment.memory_locations_
             << ", aliases=" << environment.aliases_
             << ", position=" << environment.position_
             << ", last_parameter_load=" << environment.last_parameter_load_
             << ", field_write=" << environment.field_write_ << ")";
}

} // namespace marianatrench

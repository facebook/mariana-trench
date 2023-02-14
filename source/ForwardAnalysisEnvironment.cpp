/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/ForwardAnalysisEnvironment.h>

namespace marianatrench {

namespace {

// TODO(T144485000): In the future, this will only be necessary in the backward
// analysis.
Taint propagate_artificial_sources(Taint taint, Path::Element path_element) {
  // This is called when propagating taint down in an abstract tree.
  taint.append_to_artificial_source_input_paths(path_element);
  return taint;
}

} // namespace

ForwardAnalysisEnvironment::ForwardAnalysisEnvironment()
    : memory_locations_(MemoryLocationEnvironment::bottom()),
      taint_(TaintEnvironment::bottom()),
      position_(DexPositionDomain::bottom()),
      last_parameter_load_(LastParameterLoadDomain::bottom()) {}

ForwardAnalysisEnvironment::ForwardAnalysisEnvironment(
    MemoryLocationEnvironment memory_locations,
    TaintEnvironment taint,
    DexPositionDomain position,
    LastParameterLoadDomain last_parameter_load)
    : memory_locations_(std::move(memory_locations)),
      taint_(std::move(taint)),
      position_(std::move(position)),
      last_parameter_load_(std::move(last_parameter_load)) {}

ForwardAnalysisEnvironment ForwardAnalysisEnvironment::initial() {
  return ForwardAnalysisEnvironment(
      MemoryLocationEnvironment::bottom(),
      TaintEnvironment::bottom(),
      DexPositionDomain::top(),
      LastParameterLoadDomain(0));
}

bool ForwardAnalysisEnvironment::is_bottom() const {
  return memory_locations_.is_bottom() && taint_.is_bottom() &&
      position_.is_bottom() && last_parameter_load_.is_bottom();
}

bool ForwardAnalysisEnvironment::is_top() const {
  return memory_locations_.is_top() && taint_.is_top() && position_.is_top() &&
      last_parameter_load_.is_top();
}

bool ForwardAnalysisEnvironment::leq(
    const ForwardAnalysisEnvironment& other) const {
  return memory_locations_.leq(other.memory_locations_) &&
      taint_.leq(other.taint_) && position_.leq(other.position_) &&
      last_parameter_load_.leq(other.last_parameter_load_);
}

bool ForwardAnalysisEnvironment::equals(
    const ForwardAnalysisEnvironment& other) const {
  return memory_locations_.equals(other.memory_locations_) &&
      taint_.equals(other.taint_) && position_.equals(other.position_) &&
      last_parameter_load_.equals(other.last_parameter_load_);
}

void ForwardAnalysisEnvironment::set_to_bottom() {
  memory_locations_.set_to_bottom();
  taint_.set_to_bottom();
  position_.set_to_bottom();
  last_parameter_load_.set_to_bottom();
}

void ForwardAnalysisEnvironment::set_to_top() {
  memory_locations_.set_to_top();
  taint_.set_to_top();
  position_.set_to_top();
  last_parameter_load_.set_to_top();
}

void ForwardAnalysisEnvironment::join_with(
    const ForwardAnalysisEnvironment& other) {
  mt_if_expensive_assert(auto previous = *this);

  memory_locations_.join_with(other.memory_locations_);
  taint_.join_with(other.taint_);
  position_.join_with(other.position_);
  last_parameter_load_.join_with(other.last_parameter_load_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void ForwardAnalysisEnvironment::widen_with(
    const ForwardAnalysisEnvironment& other) {
  mt_if_expensive_assert(auto previous = *this);

  memory_locations_.widen_with(other.memory_locations_);
  taint_.widen_with(other.taint_);
  position_.widen_with(other.position_);
  last_parameter_load_.widen_with(other.last_parameter_load_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void ForwardAnalysisEnvironment::meet_with(
    const ForwardAnalysisEnvironment& other) {
  memory_locations_.meet_with(other.memory_locations_);
  taint_.meet_with(other.taint_);
  position_.meet_with(other.position_);
  last_parameter_load_.meet_with(other.last_parameter_load_);
}

void ForwardAnalysisEnvironment::narrow_with(
    const ForwardAnalysisEnvironment& other) {
  memory_locations_.narrow_with(other.memory_locations_);
  taint_.narrow_with(other.taint_);
  position_.narrow_with(other.position_);
  last_parameter_load_.narrow_with(other.last_parameter_load_);
}

void ForwardAnalysisEnvironment::assign(
    Register register_id,
    MemoryLocation* memory_location) {
  memory_locations_.set(register_id, MemoryLocationsDomain{memory_location});
}

void ForwardAnalysisEnvironment::assign(
    Register register_id,
    MemoryLocationsDomain memory_locations) {
  mt_assert(!memory_locations.is_top());

  memory_locations_.set(register_id, memory_locations);
}

MemoryLocationsDomain ForwardAnalysisEnvironment::memory_locations(
    Register register_id) const {
  MemoryLocationsDomain memory_locations = memory_locations_.get(register_id);

  if (!memory_locations.is_value()) {
    // Return an empty set instead of top or bottom.
    memory_locations = {};
  }

  return memory_locations;
}

MemoryLocationsDomain ForwardAnalysisEnvironment::memory_locations(
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

TaintTree ForwardAnalysisEnvironment::read(
    MemoryLocation* memory_location) const {
  return taint_.get(memory_location->root())
      .read(memory_location->path(), propagate_artificial_sources);
}

TaintTree ForwardAnalysisEnvironment::read(
    MemoryLocation* memory_location,
    const Path& path) const {
  Path full_path = memory_location->path();
  full_path.extend(path);

  return taint_.get(memory_location->root())
      .read(full_path, propagate_artificial_sources);
}

TaintTree ForwardAnalysisEnvironment::read(
    const MemoryLocationsDomain& memory_locations) const {
  if (!memory_locations.is_value()) {
    return TaintTree::bottom();
  }

  TaintTree taint;
  for (auto* memory_location : memory_locations.elements()) {
    taint.join_with(read(memory_location));
  }
  return taint;
}

TaintTree ForwardAnalysisEnvironment::read(Register register_id) const {
  return read(memory_locations_.get(register_id));
}

TaintTree ForwardAnalysisEnvironment::read(
    Register register_id,
    const Path& path) const {
  MemoryLocationsDomain memory_locations = memory_locations_.get(register_id);

  if (!memory_locations.is_value()) {
    return TaintTree::bottom();
  }

  TaintTree taint;
  for (auto* memory_location : memory_locations.elements()) {
    taint.join_with(read(memory_location, path));
  }

  return taint;
}

void ForwardAnalysisEnvironment::write(
    MemoryLocation* memory_location,
    TaintTree taint,
    UpdateKind kind) {
  taint_.update(memory_location->root(), [&](const TaintTree& tree) {
    auto copy = tree;
    copy.write(memory_location->path(), std::move(taint), kind);
    return copy;
  });
}

void ForwardAnalysisEnvironment::write(
    MemoryLocation* memory_location,
    const Path& path,
    TaintTree taint,
    UpdateKind kind) {
  Path full_path = memory_location->path();
  full_path.extend(path);

  taint_.update(memory_location->root(), [&](const TaintTree& tree) {
    auto copy = tree;
    copy.write(full_path, std::move(taint), kind);
    return copy;
  });
}

void ForwardAnalysisEnvironment::write(
    MemoryLocation* memory_location,
    const Path& path,
    Taint taint,
    UpdateKind kind) {
  Path full_path = memory_location->path();
  full_path.extend(path);

  taint_.update(memory_location->root(), [&](const TaintTree& tree) {
    auto copy = tree;
    copy.write(full_path, std::move(taint), kind);
    return copy;
  });
}

void ForwardAnalysisEnvironment::write(
    Register register_id,
    TaintTree taint,
    UpdateKind kind) {
  MemoryLocationsDomain memory_locations = memory_locations_.get(register_id);

  if (!memory_locations.is_value()) {
    return;
  }

  if (memory_locations.size() > 1) {
    // In practice, only one of the memory location is affected, so we must
    // treat this as a weak update, even if a strong update was requested.
    kind = UpdateKind::Weak;
  }

  for (auto* memory_location : memory_locations.elements()) {
    write(memory_location, taint, kind);
  }
}

void ForwardAnalysisEnvironment::write(
    Register register_id,
    const Path& path,
    TaintTree taint,
    UpdateKind kind) {
  MemoryLocationsDomain memory_locations = memory_locations_.get(register_id);

  if (!memory_locations.is_value()) {
    return;
  }

  if (memory_locations.size() > 1) {
    // In practice, only one of the memory location is affected, so we must
    // treat this as a weak update, even if a strong update was requested.
    kind = UpdateKind::Weak;
  }

  for (auto* memory_location : memory_locations.elements()) {
    write(memory_location, path, taint, kind);
  }
}

void ForwardAnalysisEnvironment::write(
    Register register_id,
    const Path& path,
    Taint taint,
    UpdateKind kind) {
  MemoryLocationsDomain memory_locations = memory_locations_.get(register_id);

  if (!memory_locations.is_value()) {
    return;
  }

  if (memory_locations.size() > 1) {
    // In practice, only one of the memory location is affected, so we must
    // treat this as a weak update, even if a strong update was requested.
    kind = UpdateKind::Weak;
  }

  for (auto* memory_location : memory_locations.elements()) {
    write(memory_location, path, taint, kind);
  }
}

DexPosition* ForwardAnalysisEnvironment::last_position() const {
  return get_optional_value_or(position_.get_constant(), nullptr);
}

void ForwardAnalysisEnvironment::set_last_position(DexPosition* position) {
  position_ = DexPositionDomain(position);
}

const LastParameterLoadDomain&
ForwardAnalysisEnvironment::last_parameter_loaded() const {
  return last_parameter_load_;
}

void ForwardAnalysisEnvironment::increment_last_parameter_loaded() {
  if (last_parameter_load_.is_value()) {
    last_parameter_load_ =
        LastParameterLoadDomain(*last_parameter_load_.get_constant() + 1);
  }
}

std::ostream& operator<<(
    std::ostream& out,
    const ForwardAnalysisEnvironment& environment) {
  return out << "(memory_locations=" << environment.memory_locations_
             << ", taint=" << environment.taint_
             << ", position=" << environment.position_
             << ", last_parameter_load=" << environment.last_parameter_load_
             << ")";
}

} // namespace marianatrench

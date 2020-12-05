/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/AnalysisEnvironment.h>

#include <Show.h>

#include <mariana-trench/Assert.h>

namespace marianatrench {

namespace {

Taint propagate_artificial_sources(Taint taint, Path::Element path_element) {
  // This is called when propagating taint down in an abstract tree.
  taint.map([path_element](FrameSet& frames) {
    if (frames.is_artificial_sources()) {
      frames.map([path_element](Frame& frame) {
        frame.callee_port_append(path_element);
      });
    }
  });
  return taint;
}

} // namespace

AnalysisEnvironment::AnalysisEnvironment()
    : memory_locations_(MemoryLocationsPartition::bottom()),
      taint_(TaintAbstractPartition::bottom()),
      position_(DexPositionDomain::bottom()),
      last_parameter_load_(LastParameterLoadDomain::bottom()) {}

AnalysisEnvironment::AnalysisEnvironment(
    MemoryLocationsPartition memory_locations,
    TaintAbstractPartition taint,
    DexPositionDomain position,
    LastParameterLoadDomain last_parameter_load)
    : memory_locations_(std::move(memory_locations)),
      taint_(std::move(taint)),
      position_(std::move(position)),
      last_parameter_load_(std::move(last_parameter_load)) {}

AnalysisEnvironment AnalysisEnvironment::initial() {
  return AnalysisEnvironment(
      MemoryLocationsPartition::bottom(),
      TaintAbstractPartition::bottom(),
      DexPositionDomain::top(),
      LastParameterLoadDomain(0));
}

bool AnalysisEnvironment::is_bottom() const {
  return memory_locations_.is_bottom() && taint_.is_bottom() &&
      position_.is_bottom() && last_parameter_load_.is_bottom();
}

bool AnalysisEnvironment::is_top() const {
  return memory_locations_.is_top() && taint_.is_top() && position_.is_top() &&
      last_parameter_load_.is_top();
}

bool AnalysisEnvironment::leq(const AnalysisEnvironment& other) const {
  return memory_locations_.leq(other.memory_locations_) &&
      taint_.leq(other.taint_) && position_.leq(other.position_) &&
      last_parameter_load_.leq(other.last_parameter_load_);
}

bool AnalysisEnvironment::equals(const AnalysisEnvironment& other) const {
  return memory_locations_.equals(other.memory_locations_) &&
      taint_.equals(other.taint_) && position_.equals(other.position_) &&
      last_parameter_load_.equals(other.last_parameter_load_);
}

void AnalysisEnvironment::set_to_bottom() {
  memory_locations_.set_to_bottom();
  taint_.set_to_bottom();
  position_.set_to_bottom();
  last_parameter_load_.set_to_bottom();
}

void AnalysisEnvironment::set_to_top() {
  memory_locations_.set_to_top();
  taint_.set_to_top();
  position_.set_to_top();
  last_parameter_load_.set_to_top();
}

void AnalysisEnvironment::join_with(const AnalysisEnvironment& other) {
  mt_if_expensive_assert(auto previous = *this);

  memory_locations_.join_with(other.memory_locations_);
  taint_.join_with(other.taint_);
  position_.join_with(other.position_);
  last_parameter_load_.join_with(other.last_parameter_load_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void AnalysisEnvironment::widen_with(const AnalysisEnvironment& other) {
  mt_if_expensive_assert(auto previous = *this);

  memory_locations_.widen_with(other.memory_locations_);
  taint_.widen_with(other.taint_);
  position_.widen_with(other.position_);
  last_parameter_load_.widen_with(other.last_parameter_load_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void AnalysisEnvironment::meet_with(const AnalysisEnvironment& other) {
  memory_locations_.meet_with(other.memory_locations_);
  taint_.meet_with(other.taint_);
  position_.meet_with(other.position_);
  last_parameter_load_.meet_with(other.last_parameter_load_);
}

void AnalysisEnvironment::narrow_with(const AnalysisEnvironment& other) {
  memory_locations_.narrow_with(other.memory_locations_);
  taint_.narrow_with(other.taint_);
  position_.narrow_with(other.position_);
  last_parameter_load_.narrow_with(other.last_parameter_load_);
}

void AnalysisEnvironment::assign(
    Register register_id,
    MemoryLocation* memory_location) {
  memory_locations_.set(register_id, MemoryLocationsDomain{memory_location});
}

void AnalysisEnvironment::assign(
    Register register_id,
    MemoryLocationsDomain memory_locations) {
  mt_assert(!memory_locations.is_top());

  memory_locations_.set(register_id, memory_locations);
}

MemoryLocationsDomain AnalysisEnvironment::memory_locations(
    Register register_id) const {
  MemoryLocationsDomain memory_locations = memory_locations_.get(register_id);

  if (!memory_locations.is_value()) {
    // Return an empty set instead of top or bottom.
    memory_locations = {};
  }

  return memory_locations;
}

MemoryLocationsDomain AnalysisEnvironment::memory_locations(
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

TaintTree AnalysisEnvironment::read(MemoryLocation* memory_location) const {
  return taint_.get(memory_location->root())
      .read(memory_location->path(), propagate_artificial_sources);
}

TaintTree AnalysisEnvironment::read(
    MemoryLocation* memory_location,
    const Path& path) const {
  Path full_path = memory_location->path();
  full_path.extend(path);

  return taint_.get(memory_location->root())
      .read(full_path, propagate_artificial_sources);
}

TaintTree AnalysisEnvironment::read(
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

TaintTree AnalysisEnvironment::read(Register register_id) const {
  return read(memory_locations_.get(register_id));
}

TaintTree AnalysisEnvironment::read(Register register_id, const Path& path)
    const {
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

void AnalysisEnvironment::write(
    MemoryLocation* memory_location,
    TaintTree taint,
    UpdateKind kind) {
  taint_.update(memory_location->root(), [&](const TaintTree& tree) {
    auto copy = tree;
    copy.write(memory_location->path(), std::move(taint), kind);
    return copy;
  });
}

void AnalysisEnvironment::write(
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

void AnalysisEnvironment::write(
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

void AnalysisEnvironment::write(
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

void AnalysisEnvironment::write(
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

void AnalysisEnvironment::write(
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

DexPosition* AnalysisEnvironment::last_position() const {
  return get_optional_value_or(position_.get_constant(), nullptr);
}

void AnalysisEnvironment::set_last_position(DexPosition* position) {
  position_ = DexPositionDomain(position);
}

const LastParameterLoadDomain& AnalysisEnvironment::last_parameter_loaded()
    const {
  return last_parameter_load_;
}

void AnalysisEnvironment::increment_last_parameter_loaded() {
  if (last_parameter_load_.is_value()) {
    last_parameter_load_ =
        LastParameterLoadDomain(*last_parameter_load_.get_constant() + 1);
  }
}

std::ostream& operator<<(
    std::ostream& out,
    const AnalysisEnvironment& environment) {
  return out << "(memory_locations=" << environment.memory_locations_
             << ", taint=" << environment.taint_
             << ", position=" << environment.position_
             << ", last_parameter_load=" << environment.last_parameter_load_
             << ")";
}

std::ostream& operator<<(
    std::ostream& out,
    const MemoryLocationsPartition& memory_locations) {
  if (memory_locations.is_bottom()) {
    return out << "_|_";
  } else if (memory_locations.is_top()) {
    return out << "T";
  } else {
    out << "MemoryLocationsPartition(";
    for (const auto& entry : memory_locations.bindings()) {
      out << "\n  Register(" << entry.first << ") -> " << entry.second;
    }
    return out << "\n)";
  }
}

std::ostream& operator<<(
    std::ostream& out,
    const TaintAbstractPartition& taint) {
  if (taint.is_bottom()) {
    return out << "_|_";
  } else if (taint.is_top()) {
    return out << "T";
  } else {
    out << "TaintAbstractPartition(";
    for (const auto& entry : taint.bindings()) {
      out << "\n  " << show(entry.first) << " -> " << entry.second;
    }
    return out << "\n)";
  }
}

} // namespace marianatrench

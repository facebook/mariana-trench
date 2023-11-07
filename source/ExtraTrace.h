/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/CallInfo.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Kind.h>

namespace marianatrench {

/**
 * Represents the first hop of the subtraces attached to a Frame.
 */
class ExtraTrace final {
 public:
  explicit ExtraTrace(
      const Kind* kind,
      const Method* MT_NULLABLE callee,
      const Position* position,
      const AccessPath* callee_port,
      CallKind call_kind)
      : kind_(kind), call_info_(callee, call_kind, callee_port, position) {
    mt_assert(
        call_info_.call_kind().is_propagation_with_trace() &&
        kind_ != nullptr && call_info_.call_position() != nullptr);

    // Callee is nullptr iff this trace is an origin (i.e. no next hop).
    // Unlike LocalTaint, in which origins indicate where a user-declared taint
    // originates, extra traces originate from propagations, typically taint
    // transforms. These are like return sinks or parameter sources where the
    // "next hop" is either "return" or an "argument". Today, this information
    // is not stored/emitted in the extra trace.
    mt_assert(
        call_info_.call_kind().is_origin() || call_info_.callee() != nullptr);
    mt_assert(
        !call_info_.call_kind().is_origin() || call_info_.callee() == nullptr);
  }

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ExtraTrace)

  bool operator==(const ExtraTrace& other) const {
    return kind_ == other.kind_ && call_info_ == other.call_info_;
  }

  bool operator!=(const ExtraTrace& other) const {
    return !(*this == other);
  }

  const Kind* kind() const {
    return kind_;
  }

  const CallInfo& call_info() const {
    return call_info_;
  }

  const Method* MT_NULLABLE callee() const {
    return call_info_.callee();
  }

  const AccessPath* callee_port() const {
    return call_info_.callee_port();
  }

  const Position* position() const {
    return call_info_.call_position();
  }

  CallKind call_kind() const {
    return call_info_.call_kind();
  }

  Json::Value to_json() const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const ExtraTrace& extra_trace);

 private:
  const Kind* kind_;
  CallInfo call_info_;
};

} // namespace marianatrench

template <>
struct std::hash<marianatrench::ExtraTrace> {
  std::size_t operator()(const marianatrench::ExtraTrace& extra_trace) const {
    std::size_t seed = 0;

    boost::hash_combine(seed, extra_trace.kind());
    boost::hash_combine(seed, extra_trace.position());
    boost::hash_combine(seed, extra_trace.call_kind().encode());
    if (extra_trace.callee() != nullptr) {
      boost::hash_combine(seed, extra_trace.callee());
      boost::hash_combine(seed, extra_trace.callee_port());
    }

    return seed;
  }
};

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Access.h>
#include <mariana-trench/CallKind.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Position.h>

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
      : kind_(kind),
        callee_(callee),
        position_(position),
        callee_port_(callee_port),
        call_kind_(std::move(call_kind)) {
    mt_assert(
        call_kind_.is_propagation_with_trace() && kind_ != nullptr &&
        position_ != nullptr);
  }

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ExtraTrace)

  bool operator==(const ExtraTrace& other) const {
    return kind_ == other.kind_ && callee_ == other.callee_ &&
        position_ == other.position_ && callee_port_ == other.callee_port_ &&
        call_kind_ == other.call_kind_;
  }

  bool operator!=(const ExtraTrace& other) const {
    return !(*this == other);
  }

  const Kind* kind() const {
    return kind_;
  }

  const Method* MT_NULLABLE callee() const {
    return callee_;
  }

  const AccessPath* callee_port() const {
    return callee_port_;
  }

  const Position* position() const {
    return position_;
  }

  const CallKind& call_kind() const {
    return call_kind_;
  }

  Json::Value to_json() const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const ExtraTrace& extra_trace);

 private:
  const Kind* kind_;
  const Method* MT_NULLABLE callee_;
  const Position* position_;
  const AccessPath* callee_port_;
  CallKind call_kind_;
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

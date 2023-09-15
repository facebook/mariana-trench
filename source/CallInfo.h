/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Flags.h>
#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

class CallInfo final {
 public:
  using KindEncoding = unsigned int;

 public:
  /**
   * To use CallInfo along with PointerIntPair, we limit the underlying
   * representation of Kind to use only the lower 3 bits.
   * We only export named constructors for creation of CallInfo and
   * the propagate() method for mutation/state transition.
   */
  static constexpr KindEncoding Declaration = 0b000;
  static constexpr KindEncoding Origin = 0b001;
  static constexpr KindEncoding CallSite = 0b010;

 private:
  static constexpr KindEncoding Propagation = 0b011;
  static constexpr KindEncoding PropagationWithTrace = 0b100;

 private:
  explicit CallInfo(KindEncoding call_infos);

 public:
  CallInfo() = delete;

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(CallInfo)

  bool operator==(const CallInfo& other) const {
    return call_infos_ == other.call_infos_;
  }

  bool operator!=(const CallInfo& other) const {
    return !(call_infos_ == other.call_infos_);
  }

  bool is_declaration() const;

  bool is_origin() const;

  bool is_callsite() const;

  bool is_propagation() const;

  bool is_propagation_with_trace() const;

  bool is_propagation_without_trace() const;

  std::string to_trace_string() const;

  CallInfo propagate() const;

  KindEncoding encode() const {
    return call_infos_;
  }

  friend std::ostream& operator<<(std::ostream& out, const CallInfo& call_info);

 public:
  static CallInfo declaration();

  static CallInfo origin();

  static CallInfo callsite();

  static CallInfo propagation();

  static CallInfo propagation_with_trace(KindEncoding kind);

  static CallInfo decode(KindEncoding kind);

 private:
  KindEncoding call_infos_;
};

} // namespace marianatrench

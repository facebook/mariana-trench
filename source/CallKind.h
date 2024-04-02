/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>

#include <mariana-trench/Assert.h>
#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

/*
 * Represents the kind of a frame.
 *
 * Declaration: a user-declared taint frame.
 * Origin: the first frame of a trace, originated from a user-declared taint.
 * CallSite: a regular frame of a trace, originated from a given call site.
 */
class CallKind final {
 public:
  using Encoding = unsigned int;

 public:
  /**
   * To use CallKind along with PointerIntPair, we limit the underlying
   * representation of Kind to use only the lower 3 bits.
   * We only export named constructors for creation of CallKind and
   * the propagate() method for mutation/state transition.
   */
  static constexpr Encoding Declaration = 0b000;
  static constexpr Encoding Origin = 0b001;
  static constexpr Encoding CallSite = 0b010;

 private:
  static constexpr Encoding Propagation = 0b011;
  static constexpr Encoding PropagationWithTrace = 0b100;

 private:
  explicit CallKind(Encoding encoding);

 public:
  CallKind() = delete;

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(CallKind)

  bool operator==(const CallKind& other) const {
    return encoding_ == other.encoding_;
  }

  bool operator!=(const CallKind& other) const {
    return !(encoding_ == other.encoding_);
  }

  bool is_declaration() const;

  bool is_origin() const;

  bool is_callsite() const;

  bool is_propagation() const;

  bool is_propagation_with_trace() const;

  bool is_propagation_without_trace() const;

  static CallKind from_trace_string(std::string_view trace_string);
  std::string to_trace_string() const;

  CallKind propagate() const;

  Encoding encode() const {
    return encoding_;
  }

  friend std::ostream& operator<<(std::ostream& out, const CallKind& call_kind);

 public:
  static CallKind declaration() {
    return CallKind{CallKind::Declaration};
  }

  static CallKind origin() {
    return CallKind{CallKind::Origin};
  }

  static CallKind callsite() {
    return CallKind{CallKind::CallSite};
  }

  static CallKind propagation() {
    return CallKind{CallKind::Propagation};
  }

  static CallKind propagation_with_trace(Encoding kind);

  static CallKind decode(Encoding kind);

 private:
  Encoding encoding_;
};

} // namespace marianatrench

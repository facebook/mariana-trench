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

  enum class Kind : KindEncoding {
    // A declaration of taint from a model - should not be included in the final
    // trace.
    Declaration = 0x1,
    // The origin frame for taint that indicates a leaf.
    Origin = 0x2,
    // A call site where taint is propagated from some origin.
    CallSite = 0x4,
    // Special taint used to infer propagations.
    Propagation = 0x8,
    // Special taint used to infer propagations and track next hops.
    // The trace root will be paired with `Declaration`, leaf frames with
    // `Origin` and rest with `CallSite` where they will retain their respective
    // meaning when building the traces.
    PropagationWithTrace = 0x10
  };

 private:
  using CallInfoFlags = Flags<Kind>;

 private:
  explicit CallInfo(Kind kind) : call_infos_(kind) {}

  explicit CallInfo(CallInfoFlags call_infos);

 public:
  CallInfo() = delete;

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(CallInfo)

  bool operator==(const CallInfo& other) const {
    return call_infos_ == other.call_infos_;
  }

  bool operator!=(const CallInfo& other) const {
    return !(call_infos_ == other.call_infos_);
  }

  bool is_declaration() const {
    return call_infos_.test(Kind::Declaration);
  }

  bool is_origin() const {
    return call_infos_.test(Kind::Origin);
  }

  bool is_callsite() const {
    return call_infos_.test(Kind::CallSite);
  }

  bool is_propagation() const {
    return call_infos_.test(Kind::Propagation) ||
        call_infos_.test(Kind::PropagationWithTrace);
  }

  bool is_propagation_with_trace() const {
    return call_infos_.test(Kind::PropagationWithTrace);
  }

  bool is_propagation_without_trace() const {
    return call_infos_.test(Kind::Propagation);
  }

  std::string to_trace_string() const;

  CallInfo propagate() const;

  KindEncoding encode() const {
    return call_infos_.encode();
  }

  friend CallInfoFlags operator|(Kind left, Kind right);

  friend std::ostream& operator<<(std::ostream& out, const CallInfo& call_info);

 public:
  static CallInfo declaration() {
    return CallInfo{CallInfo::Kind::Declaration};
  }

  static CallInfo origin() {
    return CallInfo{CallInfo::Kind::Origin};
  }

  static CallInfo callsite() {
    return CallInfo{CallInfo::Kind::CallSite};
  }

  static CallInfo propagation() {
    return CallInfo{CallInfo::Kind::Propagation};
  }

  static CallInfo propagation_with_trace(CallInfo::Kind kind) {
    mt_assert(
        kind == CallInfo::Kind::Declaration || kind == CallInfo::Kind::Origin ||
        kind == CallInfo::Kind::CallSite);
    CallInfoFlags call_info_flags = CallInfo::Kind::PropagationWithTrace | kind;

    return CallInfo{call_info_flags};
  }

 private:
  CallInfoFlags call_infos_;
};

inline CallInfo::CallInfoFlags operator|(
    CallInfo::Kind left,
    CallInfo::Kind right) {
  return CallInfo::CallInfoFlags(left) | right;
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ostream>
#include <string>

#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

class FrameType final {
 private:
  enum class Kind : unsigned int { Source, Sink };

  explicit FrameType(Kind kind);

 public:
  FrameType() = delete;

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(FrameType)

  bool operator==(const FrameType& frame_type) const {
    return kind_ == frame_type.kind_;
  }

  static FrameType from_trace_string(std::string_view trace_string);
  std::string to_trace_string() const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const FrameType& frame_type);

  friend struct std::hash<FrameType>;

 public:
  static FrameType source() {
    return FrameType{Kind::Source};
  }

  static FrameType sink() {
    return FrameType{Kind::Sink};
  }

 private:
  Kind kind_;
};

} // namespace marianatrench

template <>
struct std::hash<marianatrench::FrameType> {
  std::size_t operator()(const marianatrench::FrameType& frame_type) const {
    return std::hash<marianatrench::FrameType::Kind>()(frame_type.kind_);
  }
};

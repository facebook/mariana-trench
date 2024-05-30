/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/FrameType.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

FrameType::FrameType(FrameType::Kind kind) : kind_(kind) {}

FrameType FrameType::from_trace_string(std::string_view trace_string) {
  if (trace_string == "source") {
    return FrameType::source();
  } else if (trace_string == "sink") {
    return FrameType::sink();
  } else {
    throw JsonValidationError(
        std::string(trace_string),
        /* field */ std::nullopt,
        "FrameType to be one of 'source' or 'sink'");
  }
}

std::string FrameType::to_trace_string() const {
  switch (kind_) {
    case FrameType::Kind::Source:
      return "source";

    case FrameType::Kind::Sink:
      return "sink";
  }
}

std::ostream& operator<<(std::ostream& out, const FrameType& frame_type) {
  return out << frame_type.to_trace_string();
}

} // namespace marianatrench

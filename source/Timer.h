/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <chrono>
#include <stdexcept>

#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

class TimeoutError : public std::runtime_error {
 public:
  explicit TimeoutError(const std::string& message, double duration_in_seconds)
      : std::runtime_error(message),
        duration_in_seconds_(duration_in_seconds) {}

  double duration_in_seconds() const {
    return duration_in_seconds_;
  }

 private:
  double duration_in_seconds_;
};

class Timer final {
 public:
  Timer() : start_(std::chrono::steady_clock::now()) {}

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Timer)

  double duration_in_seconds() const {
    double duration_in_milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_)
            .count();
    return duration_in_milliseconds / 1000.0;
  }

 private:
  std::chrono::time_point<std::chrono::steady_clock> start_;
};

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <chrono>

namespace marianatrench {

class Timer final {
 public:
  Timer() : start_(std::chrono::steady_clock::now()) {}
  Timer(const Timer&) = default;
  Timer(Timer&&) = default;
  Timer& operator=(const Timer&) = default;
  Timer& operator=(Timer&&) = default;
  ~Timer() = default;

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

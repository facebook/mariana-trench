/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>

#include <mariana-trench/Options.h>

namespace marianatrench {

class EventLogger {
 public:
  static void init_event_logger(const Options* options);
  static void log_event(
      const std::string& event,
      const std::string& message,
      int verbosity_level);

  static void log_event(
      const std::string& event,
      const std::string& message,
      int value,
      int verbosity_level);

  static void log_event(
      const std::string& event,
      const std::string& message,
      std::size_t value,
      int verbosity_level);

  static void log_event(
      const std::string& event,
      const std::string& message,
      double value,
      int verbosity_level);
};

} // namespace marianatrench

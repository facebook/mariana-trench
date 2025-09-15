/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/EventLogger.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

#ifndef MARIANA_TRENCH_FACEBOOK_BUILD
void EventLogger::log_event(
    const std::string& event,
    const std::string& message,
    int verbosity_level) {
  LOG(verbosity_level, "[{}]: Message: {}.", event, message);
}

void EventLogger::log_event(
    const std::string& event,
    const std::string& message,
    int value,
    int verbosity_level) {
  LOG(verbosity_level, "[{}]: Message: {}. Value: {}", event, message, value);
}

void EventLogger::log_event(
    const std::string& event,
    const std::string& message,
    std::size_t value,
    int verbosity_level) {
  LOG(verbosity_level, "[{}]: Message: {}. Value: {}", event, message, value);
}

void EventLogger::log_event(
    const std::string& event,
    const std::string& message,
    double value,
    int verbosity_level) {
  LOG(verbosity_level, "[{}]: Message: {}. Value: {}", event, message, value);
}

void EventLogger::init_event_logger(const Options* /* unused */) {}
#endif
} // namespace marianatrench

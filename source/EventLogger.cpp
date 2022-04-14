/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/EventLogger.h>

namespace marianatrench {

#ifndef MARIANA_TRENCH_FACEBOOK_BUILD
void EventLogger::log_event(
    const std::string& /* unused */,
    const std::string& /* unused */,
    const int /* unused */) {}

void EventLogger::init_event_logger(const Options* /* unused */) {}
#endif
} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <iostream>

#include <mariana-trench/EventLogger.h>

class ExitCode {
 private:
  enum class Code : int {
#define EXITCODE(NAME, VALUE) NAME = VALUE,
#include "ExitCodes.def"
#undef EXITCODE
  };

 public:
#define EXITCODE(NAME, VALUE)                              \
  static int NAME() {                                      \
    return static_cast<int>(Code::NAME);                   \
  }                                                        \
                                                           \
  static int NAME(const std::string& message) {            \
    std::cerr << "error: " << message << std::endl;        \
    marianatrench::EventLogger::log_event(#NAME, message); \
    return NAME();                                         \
  }
#include "ExitCodes.def"
#undef EXITCODE
};

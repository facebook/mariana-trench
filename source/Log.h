/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <string_view>

#include <fmt/format.h>
#include <fmt/ostream.h>

namespace marianatrench {

class Logger {
 public:
  static void set_level(int level);
  static int get_level();
  static bool enabled(int level);

  template <typename... Args>
  static void log(
      std::string_view section,
      int level,
      std::string_view format,
      const Args&... args) {
    log(section, level, fmt::format(fmt::runtime(format), args...));
  }

  /* Evaluates to whether the default output descriptor is interactive. */
  static bool is_interactive_output();

  static void
  log(std::string_view section, int level, std::string_view message);
};

} // namespace marianatrench

#define SECTION(section, level, format, ...)                             \
  do {                                                                   \
    if (marianatrench::Logger::enabled(level)) {                         \
      marianatrench::Logger::log(section, level, format, ##__VA_ARGS__); \
    }                                                                    \
  } while (0)

#define LOG(level, format, ...)                    \
  do {                                             \
    SECTION("INFO", level, format, ##__VA_ARGS__); \
  } while (0)

#define LOG_IF_INTERACTIVE(level, format, ...)            \
  do {                                                    \
    if (marianatrench::Logger::is_interactive_output()) { \
      LOG(level, format, ##__VA_ARGS__);                  \
    }                                                     \
  } while (0)

#define CONTEXT_LEVEL(context, level) \
  ((context) != nullptr && (context)->dump()) ? 1 : level

#define LOG_OR_DUMP(context, level, format, ...)               \
  do {                                                         \
    LOG(CONTEXT_LEVEL(context, level), format, ##__VA_ARGS__); \
  } while (0)

#define WARNING(level, format, ...)                   \
  do {                                                \
    SECTION("WARNING", level, format, ##__VA_ARGS__); \
  } while (0)

#define WARNING_OR_DUMP(context, level, format, ...)               \
  do {                                                             \
    WARNING(CONTEXT_LEVEL(context, level), format, ##__VA_ARGS__); \
  } while (0)

#define ERROR(level, format, ...)                   \
  do {                                              \
    SECTION("ERROR", level, format, ##__VA_ARGS__); \
  } while (0)

#define ERROR_OR_DUMP(context, level, format, ...)                 \
  do {                                                             \
    WARNING(CONTEXT_LEVEL(context, level), format, ##__VA_ARGS__); \
  } while (0)

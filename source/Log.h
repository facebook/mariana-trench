// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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

#ifdef NDEBUG
  static constexpr bool enabled(int level) {
    return false;
  }
#else
  static bool enabled(int level);
#endif // NDEBUG

  template <typename... Args>
  static void log(
      std::string_view section,
      int level,
      std::string_view format,
      const Args&... args) {
    log(section, level, fmt::format(format, args...));
  }

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

#define CONTEXT_LEVEL(context, level) \
  context != nullptr && context->dump() ? 1 : level

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

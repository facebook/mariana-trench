/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdio>
#include <cstdlib>
#include <mutex>

#include <mariana-trench/Log.h>

namespace {

struct LoggerImplementation {
 public:
  LoggerImplementation() : level_(0), file_(stderr) {
    const char* env = std::getenv("TRACE");
    if (env) {
      parse_environment(env);
    }
  }

  LoggerImplementation(const LoggerImplementation&) = delete;
  LoggerImplementation(LoggerImplementation&&) = delete;
  LoggerImplementation& operator=(const LoggerImplementation&) = delete;
  LoggerImplementation& operator=(LoggerImplementation&&) = delete;
  ~LoggerImplementation() = default;

  void set_level(int level) {
    level_ = level;
  }

  int get_level() {
    return level_;
  }

  bool enabled(int level) const {
    return level <= level_;
  }

  void log(std::string_view section, int level, std::string_view message) {
    if (!enabled(level)) {
      return;
    }

    std::string line = fmt::format("{} {}\n", section, message);

    std::lock_guard<std::mutex> guard(mutex_);
    fwrite(line.c_str(), line.size(), 1, file_);
    fflush(file_);
  }

 private:
  void parse_environment(std::string_view configuration) {
    // This needs to be consistent with redex.
    std::string module;
    std::string token;

    while (!configuration.empty()) {
      auto next_token_position = configuration.find_first_of(",: ");
      if (next_token_position != std::string::npos) {
        token = configuration.substr(0, next_token_position);
        configuration = configuration.substr(next_token_position + 1);
      } else {
        token = configuration;
        configuration = {};
      }

      int level = std::atoi(token.c_str());
      if (!level) {
        module = token;
      } else if (module == "MARIANA_TRENCH") {
        level_ = level;
      }
    }
  }

 private:
  int level_;
  FILE* file_;
  std::mutex mutex_;
};

static LoggerImplementation logger;

} // namespace

namespace marianatrench {

void Logger::set_level(int level) {
  logger.set_level(level);
}

int Logger::get_level() {
  return logger.get_level();
}

bool Logger::enabled(int level) {
  return logger.enabled(level);
}

void Logger::log(
    std::string_view section,
    int level,
    std::string_view message) {
  logger.log(section, level, message);
}

} // namespace marianatrench

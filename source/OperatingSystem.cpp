/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fstream>

#include <boost/algorithm/string.hpp>

#include <mariana-trench/Log.h>
#include <mariana-trench/OperatingSystem.h>

#if __APPLE__
#include <mach/mach.h>
#include <mach/mach_init.h>
#include <mach/mach_types.h>
#endif

namespace marianatrench {

double resident_set_size_in_gb() {
#if __APPLE__
  struct task_basic_info info;
  mach_msg_type_number_t info_count = TASK_BASIC_INFO_COUNT;
  int error = task_info(
      mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &info_count);
  if (error) {
    ERROR(1, "Call to `task_info()` failed");
  } else {
    return static_cast<double>(info.resident_size) / 1000.0 / 1000.0 / 1000.0;
  }
#elif __linux__
  try {
    std::ifstream infile("/proc/self/status");
    std::string line;
    while (std::getline(infile, line)) {
      if (boost::starts_with(line, "VmRSS:")) {
        return static_cast<double>(std::stoll(line.substr(7), nullptr)) /
            1000.0 / 1000.0;
      }
    }
    ERROR(1, "Resident set size not found in `/proc/self/status`");
  } catch (const std::exception& error) {
    ERROR(1, "Failed to read `/proc/self/status`: {}", error.what());
  }
#endif
  return -1.0;
}

} // namespace marianatrench

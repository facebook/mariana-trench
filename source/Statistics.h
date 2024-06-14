/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mutex>
#include <unordered_map>
#include <vector>

#include <json/json.h>

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Timer.h>

namespace marianatrench {

/**
 * Record various statistics during the analysis.
 */
class Statistics final {
 public:
  Statistics() = default;

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Statistics)

  void log_number_iterations(std::size_t number_iterations);
  void log_resident_set_size(double resident_set_size);
  void log_time(const std::string& name, const Timer& timer);
  void log_time(const Method* method, const Timer& timer);
  void log_unable_to_resolve_call(const DexMethodRef* method);

  Json::Value to_json() const;

  /* Maximum number of slowest methods to record. */
  constexpr static std::size_t kRecordSlowestMethods = 20;

  const std::unordered_set<const DexMethodRef*>& unresolved_methods() const;

 private:
  std::mutex mutex_;

  // Final number of iterations.
  std::size_t number_iterations_ = 0;

  // Maximum RSS, in GB.
  double max_resident_set_size_ = -1.0;

  // Recorded times for each step of the analysis.
  std::unordered_map<std::string, double> times_;

  //  Sorted list of slowest methods to analyze (from slowest to fastest).
  std::vector<std::pair<const Method*, double>> slowest_methods_;

  // Set of methods that could not be resolved.
  std::unordered_set<const DexMethodRef*> unresolved_methods_ =
      std::unordered_set<const DexMethodRef*>();
};

} // namespace marianatrench

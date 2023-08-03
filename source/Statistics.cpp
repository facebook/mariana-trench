/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>

#include <sparta/WorkQueue.h>

#include <Show.h>

#include <mariana-trench/Statistics.h>

namespace marianatrench {

void Statistics::log_number_iterations(std::size_t number_iterations) {
  std::lock_guard<std::mutex> lock(mutex_);
  number_iterations_ = number_iterations;
}

void Statistics::log_resident_set_size(double resident_set_size) {
  std::lock_guard<std::mutex> lock(mutex_);
  max_resident_set_size_ = std::max(max_resident_set_size_, resident_set_size);
}

void Statistics::log_time(const std::string& name, const Timer& timer) {
  std::lock_guard<std::mutex> lock(mutex_);
  times_[name] = timer.duration_in_seconds();
}

void Statistics::log_time(const Method* method, const Timer& timer) {
  double duration_in_seconds = timer.duration_in_seconds();

  std::lock_guard<std::mutex> lock(mutex_);

  if (slowest_methods_.size() >= Statistics::kRecordSlowestMethods &&
      slowest_methods_.back().second > duration_in_seconds) {
    return;
  }

  auto found = std::find_if(
      slowest_methods_.begin(),
      slowest_methods_.end(),
      [=](const auto& record) { return record.first == method; });
  if (found != slowest_methods_.end()) {
    slowest_methods_.erase(found);
  } else if (slowest_methods_.size() >= Statistics::kRecordSlowestMethods) {
    slowest_methods_.pop_back();
  }

  auto record = std::make_pair(method, duration_in_seconds);
  slowest_methods_.insert(
      std::upper_bound(
          slowest_methods_.begin(),
          slowest_methods_.end(),
          record,
          [](const auto& left, const auto& right) {
            return left.second > right.second;
          }),
      record);
}

namespace {

double round(double x, int digits) {
  double y = std::pow(10, digits);
  return std::round(x * y) / y;
}

} // namespace

Json::Value Statistics::to_json() const {
  auto value = Json::Value(Json::objectValue);
  value["iterations"] =
      Json::Value(static_cast<Json::UInt64>(number_iterations_));
  value["rss"] = Json::Value(round(max_resident_set_size_, 6));
  value["cores"] = Json::Value(sparta::parallel::default_num_threads());

  auto times_value = Json::Value(Json::objectValue);
  for (const auto& record : times_) {
    times_value[record.first] = Json::Value(round(record.second, 3));
  }
  value["times"] = times_value;

  auto slowest_methods_value = Json::Value(Json::arrayValue);
  for (const auto& record : slowest_methods_) {
    auto slow_method_value = Json::Value(Json::arrayValue);
    slow_method_value.append(Json::Value(show(record.first)));
    slow_method_value.append(Json::Value(round(record.second, 3)));
    slowest_methods_value.append(slow_method_value);
  }
  value["slowest_methods"] = slowest_methods_value;

  return value;
}

} // namespace marianatrench

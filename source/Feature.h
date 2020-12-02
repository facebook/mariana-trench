// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <ostream>
#include <string>

#include <json/json.h>

#include <mariana-trench/Context.h>

namespace marianatrench {

class Feature final {
 public:
  explicit Feature(std::string data) : data_(std::move(data)) {}
  Feature(const Feature&) = delete;
  Feature(Feature&&) = delete;
  Feature& operator=(const Feature&) = delete;
  Feature& operator=(Feature&&) = delete;
  ~Feature() = default;

  static const Feature* from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

 private:
  friend std::ostream& operator<<(std::ostream& out, const Feature& feature);

  std::string data_;
};

} // namespace marianatrench

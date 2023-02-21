/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ostream>
#include <string>

#include <json/json.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

class Feature final {
 public:
  explicit Feature(std::string name) : name_(std::move(name)) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Feature)

  static const Feature* from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

  const std::string& name() const {
    return name_;
  }

 private:
  friend std::ostream& operator<<(std::ostream& out, const Feature& feature);

  std::string name_;
};

} // namespace marianatrench

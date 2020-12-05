/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ostream>
#include <string>

#include <json/json.h>

#include <mariana-trench/Context.h>

namespace marianatrench {

/**
 * The kind of a source or a sink (e.g, UserControlledInput)
 */
class Kind final {
 public:
  explicit Kind(std::string name) : name_(std::move(name)) {}
  Kind(const Kind&) = delete;
  Kind(Kind&&) = delete;
  Kind& operator=(const Kind&) = delete;
  Kind& operator=(Kind&&) = delete;
  ~Kind() = default;

  static const Kind* from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

 private:
  friend std::ostream& operator<<(std::ostream& out, const Kind& kind);

  std::string name_;
};

} // namespace marianatrench

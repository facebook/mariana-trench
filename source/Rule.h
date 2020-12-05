/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <unordered_set>

#include <gtest/gtest_prod.h>
#include <json/json.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/Kind.h>

namespace marianatrench {

class Rule final {
 public:
  Rule(
      const std::string& name,
      int code,
      const std::string& description,
      const std::unordered_set<const Kind*>& source_kinds,
      const std::unordered_set<const Kind*>& sink_kinds)
      : name_(name),
        code_(code),
        description_(description),
        source_kinds_(source_kinds),
        sink_kinds_(sink_kinds) {}
  Rule(const Rule&) = default;
  Rule(Rule&&) = default;
  Rule& operator=(const Rule&) = default;
  Rule& operator=(Rule&&) = default;
  ~Rule() = default;

  const std::string& name() const {
    return name_;
  }

  int code() const {
    return code_;
  }

  const std::unordered_set<const Kind*>& source_kinds() const {
    return source_kinds_;
  }

  const std::unordered_set<const Kind*>& sink_kinds() const {
    return sink_kinds_;
  }

  static Rule from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

  FRIEND_TEST(JsonTest, Rule);

 private:
  std::string name_;
  int code_;
  std::string description_;
  std::unordered_set<const Kind*> source_kinds_;
  std::unordered_set<const Kind*> sink_kinds_;
};

} // namespace marianatrench

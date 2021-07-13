/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include <gtest/gtest_prod.h>
#include <json/json.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Kind.h>

namespace marianatrench {

class Rule {
 public:
  using KindSet = std::unordered_set<const Kind*>;

  Rule(const std::string& name, int code, const std::string& description)
      : name_(name), code_(code), description_(description) {}
  Rule(const Rule&) = delete;
  Rule(Rule&&) = delete;
  Rule& operator=(const Rule&) = delete;
  Rule& operator=(Rule&&) = delete;
  virtual ~Rule() = default;

  const std::string& name() const {
    return name_;
  }

  int code() const {
    return code_;
  }

  const std::string& description() const {
    return description_;
  }

  virtual bool uses(const Kind*) const = 0;

  template <typename T>
  const T* MT_NULLABLE as() const {
    return dynamic_cast<const T*>(this);
  }

  template <typename T>
  T* MT_NULLABLE as() {
    return dynamic_cast<T*>(this);
  }

  static std::unique_ptr<Rule> from_json(
      const Json::Value& value,
      Context& context);
  virtual Json::Value to_json() const;

 private:
  std::string name_;
  int code_;
  std::string description_;
};

} // namespace marianatrench

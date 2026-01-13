/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
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
#include <mariana-trench/TransformList.h>

namespace marianatrench {

struct CoveredRule;

class Rule {
 public:
  using KindSet = std::unordered_set<const Kind*>;
  using TransformSet = std::unordered_set<const Transform*>;

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

  /**
   * A rule is "covered" by a set of kinds/transforms if it can be triggered
   * by some combination of them. Perhaps more clearly, a rule is not covered
   * if some kind/transform required for a rule to fire is missing. E.g. Rule
   * requires SourceA and SinkB, but SinkB is not a valid sink. This rule is
   * uncovered. Returns `std::nullopt` if non-covered rules. Otherwise, returns
   * the coverage information containing the specific kinds/transforms that
   * result in it being considered "covered".
   */
  virtual std::optional<CoveredRule> coverage(
      const KindSet& sources,
      const KindSet& sinks,
      const TransformSet& transforms) const = 0;

  template <typename T>
  const T* MT_NULLABLE as() const {
    static_assert(std::is_base_of<Rule, T>::value, "invalid as<T>");
    return dynamic_cast<const T*>(this);
  }

  template <typename T>
  T* MT_NULLABLE as() {
    static_assert(std::is_base_of<Rule, T>::value, "invalid as<T>");
    return dynamic_cast<T*>(this);
  }

  /**
   * Used as a helper for implementation of used_[sources|sinks|transforms].
   * Generally only makes sense to be called if the rule itself has non-empty
   * [sources|sinks|transforms].
   */
  template <typename T>
  static std::unordered_set<const T*> intersecting_kinds(
      const std::unordered_set<const T*>& rule_kinds,
      const std::unordered_set<const T*>& kinds) {
    mt_assert(rule_kinds.size() > 0);
    std::unordered_set<const T*> intersection;

    // Iterate on the likely smaller set (rule_kinds)
    for (const auto* kind : rule_kinds) {
      auto used_kind = kinds.find(kind);
      if (used_kind == kinds.end()) {
        continue;
      }
      intersection.insert(*used_kind);
    }

    return intersection;
  }

  static std::unique_ptr<Rule> from_json(
      const Json::Value& value,
      Context& context);

  static std::unique_ptr<Rule> from_json(
      const std::string& name,
      int code,
      const std::string& description,
      const Json::Value& value,
      Context& context);

  virtual Json::Value to_json(bool include_metadata) const;

 private:
  std::string name_;
  int code_;
  std::string description_;
};

} // namespace marianatrench

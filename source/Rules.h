// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <unordered_map>
#include <vector>

#include <boost/iterator/transform_iterator.hpp>
#include <json/json.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/Rule.h>

namespace marianatrench {

class Rules final {
 private:
  struct ExposeRulePointer {
    const Rule* operator()(const std::pair<const int, Rule>& pair) const {
      return &pair.second;
    }
  };

 public:
  // C++ container concept member types
  using iterator = boost::transform_iterator<
      ExposeRulePointer,
      std::unordered_map<int, Rule>::const_iterator>;
  using const_iterator = iterator;
  using value_type = const Rule*;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using const_reference = const Rule*;
  using const_pointer = typename iterator::pointer;

 private:
  // Safety checks of `boost::transform_iterator`.
  static_assert(std::is_same_v<typename iterator::value_type, value_type>);
  static_assert(
      std::is_same_v<typename iterator::difference_type, difference_type>);
  static_assert(std::is_same_v<typename iterator::reference, const_reference>);

 public:
  explicit Rules();

  explicit Rules(const std::vector<Rule>& rules);

  explicit Rules(Context& context, const Json::Value& rules_value);

  /* Load the rules from the json file specified in the given options. */
  static Rules load(Context& context, const Options& options);

  Rules(const Rules&) = delete;
  Rules(Rules&&) = default;
  Rules& operator=(const Rules&) = delete;
  Rules& operator=(Rules&&) = delete;
  ~Rules() = default;

  /* This is NOT thread-safe. */
  void add(const Rule& rule);

  /* Return the set of rules matching the given source kind and sink kind. */
  const std::vector<const Rule*>& rules(
      const Kind* source_kind,
      const Kind* sink_kind) const;

  void warn_unused_kinds(const Kinds& kinds) const;

  std::size_t size() const {
    return rules_.size();
  }

  iterator begin() const {
    return boost::make_transform_iterator(rules_.cbegin(), ExposeRulePointer());
  }

  iterator end() const {
    return boost::make_transform_iterator(rules_.cend(), ExposeRulePointer());
  }

 private:
  std::unordered_map<int, Rule> rules_;
  std::unordered_map<
      const Kind*,
      std::unordered_map<const Kind*, std::vector<const Rule*>>>
      source_to_sink_to_rules_;
  std::vector<const Rule*> empty_rule_set;
};

} // namespace marianatrench

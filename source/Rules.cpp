/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Kinds.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Rules.h>

namespace marianatrench {

Rules::Rules() = default;

Rules::Rules(const std::vector<Rule>& rules) {
  for (const auto& rule : rules) {
    add(rule);
  }
}

Rules::Rules(Context& context, const Json::Value& rules_value) {
  for (const auto& rule_value : JsonValidation::null_or_array(rules_value)) {
    add(Rule::from_json(rule_value, context));
  }
}

Rules Rules::load(Context& context, const Options& options) {
  Rules rules;

  for (const auto& rules_path : options.rules_paths()) {
    auto rules_value = JsonValidation::parse_json_file(rules_path);
    for (const auto& rule_value : JsonValidation::null_or_array(rules_value)) {
      rules.add(Rule::from_json(rule_value, context));
    }
  }

  rules.warn_unused_kinds(*context.kinds);
  return rules;
}

void Rules::add(const Rule& rule) {
  auto result = rules_.emplace(rule.code(), rule);
  const Rule* rule_pointer = &result.first->second;

  if (!result.second) {
    ERROR(
        1,
        "A rule for code {} already exists! Duplicate rules are:\n{}\n{}",
        rule.code(),
        rule.to_json().toStyledString(),
        rule_pointer->to_json().toStyledString());
    return;
  }

  for (const auto* source_kind : rule.source_kinds()) {
    for (const auto sink_kind : rule.sink_kinds()) {
      source_to_sink_to_rules_[source_kind][sink_kind].push_back(rule_pointer);
    }
  }
}

const std::vector<const Rule*>& Rules::rules(
    const Kind* source_kind,
    const Kind* sink_kind) const {
  auto sink_to_rules = source_to_sink_to_rules_.find(source_kind);
  if (sink_to_rules == source_to_sink_to_rules_.end()) {
    return empty_rule_set;
  }

  auto rules = sink_to_rules->second.find(sink_kind);
  if (rules == sink_to_rules->second.end()) {
    return empty_rule_set;
  }

  return rules->second;
}

void Rules::warn_unused_kinds(const Kinds& kinds) const {
  for (const auto* kind : kinds) {
    if (std::all_of(begin(), end(), [kind](const Rule* rule) {
          return rule->source_kinds().count(kind) == 0 &&
              rule->sink_kinds().count(kind) == 0;
        })) {
      WARNING(
          1,
          "Kind `{}` is not used in any rule! You may want to add one for it.",
          show(kind));
    }
  }
}

} // namespace marianatrench

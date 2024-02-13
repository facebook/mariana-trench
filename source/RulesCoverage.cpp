/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/RulesCoverage.h>

namespace marianatrench {

bool CoveredRule::operator==(const CoveredRule& other) const {
  return code == other.code && used_sources == other.used_sources &&
      used_sinks == other.used_sinks &&
      used_transforms == other.used_transforms;
}

bool RulesCoverage::operator==(const RulesCoverage& other) const {
  return covered_rules == other.covered_rules &&
      non_covered_rule_codes == other.non_covered_rule_codes;
}

RulesCoverage RulesCoverage::create(
    const Rules& rules,
    const Rule::KindSet& used_sources,
    const Rule::KindSet& used_sinks,
    const Rule::TransformSet& used_transforms) {
  RulesCoverage coverage;
  for (const auto* rule : rules) {
    auto used_sources_for_rule = rule->used_sources(used_sources);
    if (used_sources_for_rule.empty()) {
      coverage.non_covered_rule_codes.insert(rule->code());
      continue;
    }

    auto used_sinks_for_rule = rule->used_sinks(used_sinks);
    if (used_sinks_for_rule.empty()) {
      coverage.non_covered_rule_codes.insert(rule->code());
      continue;
    }

    Rule::TransformSet used_transforms_for_rule;
    auto rule_transforms = rule->transforms();
    if (rule_transforms.size() > 0) {
      // Only check intersection with used_transforms if the rule uses them.
      // Otherwise the intersection will always be empty.
      used_transforms_for_rule = rule->used_transforms(used_transforms);
      if (used_transforms_for_rule.empty()) {
        coverage.non_covered_rule_codes.insert(rule->code());
        continue;
      }
    }

    coverage.covered_rules.emplace(
        rule->code(),
        CoveredRule{
            .code = rule->code(),
            .used_sources = used_sources_for_rule,
            .used_sinks = used_sinks_for_rule,
            .used_transforms = used_transforms_for_rule});
  }
  return coverage;
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/RulesCoverage.h>

namespace marianatrench {

namespace {
template <typename T>
Json::Value to_json_array(
    const std::unordered_set<T>& primitive_set,
    const std::function<Json::Value(T)>& to_json_value) {
  auto result = Json::Value(Json::arrayValue);
  for (auto element : primitive_set) {
    result.append(to_json_value(element));
  }
  return result;
}
} // namespace

bool CoveredRule::operator==(const CoveredRule& other) const {
  return code == other.code && used_sources == other.used_sources &&
      used_sinks == other.used_sinks &&
      used_transforms == other.used_transforms;
}

Json::Value CoveredRule::to_json() const {
  auto result = Json::Value(Json::objectValue);
  result["code"] = code;

  auto cases = Json::Value(Json::objectValue);
  auto kind_to_json = [](const Kind* kind) {
    return Json::Value(kind->to_trace_string());
  };

  cases["sources"] = to_json_array<const Kind*>(used_sources, kind_to_json);
  cases["sinks"] = to_json_array<const Kind*>(used_sinks, kind_to_json);
  if (!used_transforms.empty()) {
    cases["transforms"] = to_json_array<const Transform*>(
        used_transforms, [](const Transform* transform) {
          return Json::Value(transform->to_trace_string());
        });
  }
  result["cases"] = cases;
  return result;
}

bool RulesCoverage::operator==(const RulesCoverage& other) const {
  return covered_rules == other.covered_rules &&
      non_covered_rule_codes == other.non_covered_rule_codes;
}

Json::Value RulesCoverage::to_json() const {
  auto category_coverage = Json::Value(Json::objectValue);

  auto rules_covered = Json::Value(Json::arrayValue);
  for (const auto& [_code, covered_rule] : covered_rules) {
    rules_covered.append(covered_rule.to_json());
  }
  category_coverage["rules_covered"] = rules_covered;

  auto rules_lacking_models = Json::Value(Json::arrayValue);
  category_coverage["rules_lacking_models"] = to_json_array<int>(
      non_covered_rule_codes, [](int code) { return Json::Value(code); });

  auto result = Json::Value(Json::objectValue);
  result["category_coverage"] = category_coverage;
  return result;
}

RulesCoverage RulesCoverage::create(
    const Rules& rules,
    const Rule::KindSet& used_sources,
    const Rule::KindSet& used_sinks,
    const Rule::TransformSet& used_transforms) {
  RulesCoverage coverage;
  for (const auto* rule : rules) {
    auto covered_rule =
        rule->coverage(used_sources, used_sinks, used_transforms);
    if (!covered_rule.has_value()) {
      coverage.non_covered_rule_codes.insert(rule->code());
    } else {
      coverage.covered_rules.emplace(rule->code(), *covered_rule);
    }
  }

  return coverage;
}

} // namespace marianatrench

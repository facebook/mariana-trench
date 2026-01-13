/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/MultiCaseRule.h>
#include <mariana-trench/NamedKind.h>
#include <mariana-trench/Rule.h>
#include <mariana-trench/RulesCoverage.h>
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {

bool MultiCaseRule::uses(const Kind* kind) const {
  return std::any_of(
      cases_.begin(), cases_.end(), [kind](const auto& rule_case) {
        return rule_case->uses(kind);
      });
}

std::optional<CoveredRule> MultiCaseRule::coverage(
    const KindSet& sources,
    const KindSet& sinks,
    const TransformSet& transforms) const {
  std::vector<CoveredRule> covered_cases;
  for (const auto& rule_case : cases_) {
    auto covered_case = rule_case->coverage(sources, sinks, transforms);
    if (covered_case.has_value()) {
      covered_cases.push_back(*covered_case);
    }
  }

  if (covered_cases.empty()) {
    return std::nullopt;
  }

  KindSet all_used_sources;
  KindSet all_used_sinks;
  TransformSet all_used_transforms;

  for (auto& covered_case : covered_cases) {
    all_used_sources.merge(covered_case.used_sources);
    all_used_sinks.merge(covered_case.used_sinks);
    all_used_transforms.merge(covered_case.used_transforms);
  }

  return CoveredRule{
      .code = code(),
      .used_sources = std::move(all_used_sources),
      .used_sinks = std::move(all_used_sinks),
      .used_transforms = std::move(all_used_transforms),
  };
}

std::unique_ptr<Rule> MultiCaseRule::from_json(
    const std::string& name,
    int code,
    const std::string& description,
    const Json::Value& value,
    Context& context) {
  std::vector<std::unique_ptr<Rule>> cases;
  for (const auto& case_value :
       JsonValidation::nonempty_array(value, /* field */ "cases")) {
    JsonValidation::check_invalid_members(
        case_value, {"name", "code", "description", "cases", "oncall"});

    cases.push_back(
        Rule::from_json(name, code, description, case_value, context));
  }

  return std::make_unique<MultiCaseRule>(
      name, code, description, std::move(cases));
}

Json::Value MultiCaseRule::to_json(bool include_metadata) const {
  auto value = Rule::to_json(include_metadata);

  auto cases_value = Json::Value(Json::arrayValue);
  for (const auto& rule_case : cases_) {
    cases_value.append(rule_case->to_json(/* include_metadata */ false));
  }

  value["cases"] = cases_value;

  return value;
}

} // namespace marianatrench

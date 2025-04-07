/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonReaderWriter.h>
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

std::ostream& operator<<(std::ostream& out, const CoveredRule& covered_rule) {
  out << "Rule(code=" << covered_rule.code << ", sources={";
  for (const auto* source : covered_rule.used_sources) {
    out << source->to_trace_string() << ",";
  }
  out << "}, sinks={";
  for (const auto* sink : covered_rule.used_sinks) {
    out << sink->to_trace_string() << ",";
  }
  out << "}, transforms={";
  for (const auto* transforms : covered_rule.used_transforms) {
    out << transforms->to_trace_string() << ",";
  }
  out << "})";
  return out;
}

bool RulesCoverage::operator==(const RulesCoverage& other) const {
  return covered_rules_ == other.covered_rules_ &&
      non_covered_rule_codes_ == other.non_covered_rule_codes_;
}

Json::Value RulesCoverage::to_json() const {
  auto category_coverage = Json::Value(Json::objectValue);

  auto rules_covered = Json::Value(Json::arrayValue);
  for (const auto& [_code, covered_rule] : covered_rules_) {
    rules_covered.append(covered_rule.to_json());
  }
  category_coverage["rules_covered"] = rules_covered;

  auto rules_lacking_models = Json::Value(Json::arrayValue);
  category_coverage["rules_lacking_models"] = to_json_array<int>(
      non_covered_rule_codes_, [](int code) { return Json::Value(code); });

  auto result = Json::Value(Json::objectValue);
  result["category_coverage"] = category_coverage;
  return result;
}

RulesCoverage RulesCoverage::compute(
    const Registry& registry,
    const Rules& rules) {
  auto used_sources = registry.compute_used_sources();
  auto used_sinks = registry.compute_used_sinks();
  auto used_transforms = registry.compute_used_transforms();

  std::unordered_map<int, CoveredRule> covered_rules;
  std::unordered_set<int> non_covered_rule_codes;
  for (const auto* rule : rules) {
    auto covered_rule =
        rule->coverage(used_sources, used_sinks, used_transforms);
    if (!covered_rule.has_value()) {
      non_covered_rule_codes.insert(rule->code());
    } else {
      covered_rules.emplace(rule->code(), *covered_rule);
    }
  }
  return RulesCoverage(
      std::move(covered_rules), std::move(non_covered_rule_codes));
}

void RulesCoverage::dump(const std::filesystem::path& output_path) const {
  JsonWriter::write_json_file(output_path, to_json());
}

std::ostream& operator<<(std::ostream& out, const RulesCoverage& coverage) {
  out << "RulesCoverage(covered_rules={";
  for (const auto& [_code, covered_rule] : coverage.covered_rules_) {
    out << covered_rule << ",";
  }
  out << "}, non_covered_rules={";
  for (auto code : coverage.non_covered_rule_codes_) {
    out << code << ",";
  }
  out << "})";
  return out;
}
} // namespace marianatrench

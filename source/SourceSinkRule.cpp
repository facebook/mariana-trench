/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/NamedKind.h>
#include <mariana-trench/Rule.h>
#include <mariana-trench/RulesCoverage.h>
#include <mariana-trench/SourceSinkRule.h>
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {

bool SourceSinkRule::uses(const Kind* kind) const {
  const auto* base_kind = kind->discard_transforms()->discard_subkind();
  return source_kinds_.count(base_kind) != 0 ||
      sink_kinds_.count(base_kind) != 0;
}

std::optional<CoveredRule> SourceSinkRule::coverage(
    const KindSet& sources,
    const KindSet& sinks,
    const TransformSet& transforms) const {
  auto used_rule_sources =
      Rule::intersecting_kinds(/* rule_kinds */ source_kinds(), sources);
  if (used_rule_sources.empty()) {
    return std::nullopt;
  }

  auto used_rule_sinks =
      Rule::intersecting_kinds(/* rule_kinds */ sink_kinds(), sinks);
  if (used_rule_sinks.empty()) {
    return std::nullopt;
  }

  TransformSet used_rule_transforms;
  // Not all rules have transforms. Only check for transform usage if the
  // rule actually involves these.
  if (transforms_ != nullptr) {
    auto rule_transforms =
        TransformSet(transforms_->begin(), transforms_->end());
    used_rule_transforms =
        Rule::intersecting_kinds(rule_transforms, transforms);
    if (used_rule_transforms.empty()) {
      return std::nullopt;
    }
  }

  return CoveredRule{
      .code = code(),
      .used_sources = std::move(used_rule_sources),
      .used_sinks = std::move(used_rule_sinks),
      .used_transforms = std::move(used_rule_transforms),
  };
}

std::unique_ptr<Rule> SourceSinkRule::from_json(
    const std::string& name,
    int code,
    const std::string& description,
    const Json::Value& value,
    Context& context) {
  JsonValidation::check_unexpected_members(
      value,
      {"name",
       "code",
       "description",
       "sources",
       "sinks",
       "transforms",
       "oncall"});

  KindSet source_kinds;
  for (const auto& source_kind :
       JsonValidation::nonempty_array(value, /* field */ "sources")) {
    source_kinds.insert(NamedKind::from_rule_json(source_kind, context));
  }

  KindSet sink_kinds;
  for (const auto& sink_kind :
       JsonValidation::nonempty_array(value, /* field */ "sinks")) {
    sink_kinds.insert(NamedKind::from_rule_json(sink_kind, context));
  }

  const TransformList* transforms = nullptr;
  if (value.isMember("transforms")) {
    transforms = context.transforms_factory->create(
        TransformList::from_json(value["transforms"], context));
  }

  return std::make_unique<SourceSinkRule>(
      name, code, description, source_kinds, sink_kinds, transforms);
}

Json::Value SourceSinkRule::to_json(bool include_metadata) const {
  auto value = Rule::to_json(include_metadata);
  auto source_kinds_value = Json::Value(Json::arrayValue);
  for (const auto* source_kind : source_kinds_) {
    source_kinds_value.append(source_kind->to_json());
  }

  auto sink_kinds_value = Json::Value(Json::arrayValue);
  for (const auto* sink_kind : sink_kinds_) {
    sink_kinds_value.append(sink_kind->to_json());
  }

  value["sources"] = source_kinds_value;
  value["sinks"] = sink_kinds_value;

  if (transforms_ == nullptr) {
    return value;
  }

  auto transforms_value = Json::Value(Json::arrayValue);
  for (const auto* transform : *transforms_) {
    transforms_value.append(transform->to_trace_string());
  }

  value["transforms"] = transforms_value;

  return value;
}

} // namespace marianatrench

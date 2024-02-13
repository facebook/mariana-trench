/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/MultiSourceMultiSinkRule.h>
#include <mariana-trench/NamedKind.h>
#include <mariana-trench/PartialKind.h>
#include <mariana-trench/Rule.h>

namespace marianatrench {

MultiSourceMultiSinkRule::PartialKindSet
MultiSourceMultiSinkRule::partial_sink_kinds(const std::string& label) const {
  PartialKindSet result;
  for (const auto* sink_kind : partial_sink_kinds_) {
    if (sink_kind->label() == label) {
      result.insert(sink_kind);
    }
  }
  return result;
}

bool MultiSourceMultiSinkRule::uses(const Kind* kind) const {
  for (const auto& [_label, kinds] : multi_source_kinds_) {
    if (kinds.count(kind) != 0) {
      return true;
    }
  }

  return partial_sink_kinds_.count(kind->as<PartialKind>()) != 0;
}

Rule::KindSet MultiSourceMultiSinkRule::used_sources(
    const KindSet& sources) const {
  KindSet rule_sources;
  for (const auto& [_label, kinds] : multi_source_kinds_) {
    rule_sources.insert(kinds.begin(), kinds.end());
  }
  return Rule::intersecting_kinds(rule_sources, sources);
}

Rule::KindSet MultiSourceMultiSinkRule::used_sinks(const KindSet& sinks) const {
  return Rule::intersecting_kinds(
      /* rule_kinds */ KindSet(
          partial_sink_kinds_.begin(), partial_sink_kinds_.end()),
      sinks);
}

std::unique_ptr<Rule> MultiSourceMultiSinkRule::from_json(
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
       "multi_sources",
       "partial_sinks",
       "oncall"});

  const auto& sources = JsonValidation::object(value, "multi_sources");
  const auto& labels = sources.getMemberNames();

  MultiSourceKindsByLabel multi_source_kinds;
  for (const auto& label : labels) {
    KindSet kinds;
    for (const auto& kind : JsonValidation::nonempty_array(sources, label)) {
      kinds.insert(NamedKind::from_json(kind, context));
    }
    multi_source_kinds.emplace(label, std::move(kinds));
  }

  if (multi_source_kinds.size() != 2) {
    throw JsonValidationError(
        value,
        "multi_sources",
        "exactly 2 labels (as JSON object keys) in the multi_sources object");
  }

  PartialKindSet partial_sink_kinds;
  for (const auto& sink_kind :
       JsonValidation::nonempty_array(value, "partial_sinks")) {
    for (const auto& label : labels) {
      partial_sink_kinds.insert(
          PartialKind::from_json(sink_kind, label, context));
    }
  }

  return std::make_unique<MultiSourceMultiSinkRule>(
      name, code, description, multi_source_kinds, partial_sink_kinds);
}

Json::Value MultiSourceMultiSinkRule::to_json() const {
  auto value = Rule::to_json();
  auto multi_sources_value = Json::Value(Json::objectValue);
  for (const auto& [label, source_kinds] : multi_source_kinds_) {
    auto source_kinds_value = Json::Value(Json::arrayValue);
    for (const auto* source_kind : source_kinds) {
      source_kinds_value.append(source_kind->to_json());
    }
    multi_sources_value[label] = source_kinds_value;
  }

  auto partial_sinks_value = Json::Value(Json::arrayValue);
  for (const auto* partial_sink_kind : partial_sink_kinds_) {
    partial_sinks_value.append(partial_sink_kind->to_json());
  }

  value["multi_sources"] = multi_sources_value;
  value["partial_sinks"] = partial_sinks_value;
  return value;
}

} // namespace marianatrench

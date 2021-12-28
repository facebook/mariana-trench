/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/FulfilledPartialKindState.h>
#include <optional>

namespace marianatrench {

std::optional<FrameSet> FulfilledPartialKindState::fulfill_kind(
    const PartialKind* kind,
    const MultiSourceMultiSinkRule* rule,
    const FeatureMayAlwaysSet& features,
    MethodContext* context,
    const FrameSet& sink) {
  const auto* counterpart = get_fulfilled_counterpart(kind, rule);
  if (counterpart) {
    // If both partial sinks for the callsite have been fulfilled, the rule
    // is satisfied. Make this a triggered sink and create the sink flow
    // (FrameSet) for the issue. Include the features from both flows.
    mt_assert(sink.kind() == kind);
    auto issue_sink = sink.with_kind(context->kinds.get_triggered(kind, rule));
    issue_sink.add_inferred_features(get_features(counterpart, rule));
    issue_sink.add_inferred_features(features);
    erase(counterpart, rule);
    return issue_sink;
  }

  add_fulfilled_kind(kind, rule, features);
  return std::nullopt;
}

const PartialKind* MT_NULLABLE
FulfilledPartialKindState::get_fulfilled_counterpart(
    const PartialKind* unfulfilled_kind,
    const MultiSourceMultiSinkRule* rule) const {
  for (const auto& [kind, rules_map] : map_) {
    if (unfulfilled_kind->is_counterpart(kind) &&
        rules_map.find(rule) != rules_map.end()) {
      return kind;
    }
  }

  return nullptr;
}

FeatureMayAlwaysSet FulfilledPartialKindState::get_features(
    const PartialKind* kind,
    const MultiSourceMultiSinkRule* rule) const {
  return map_.at(kind).at(rule);
}

std::vector<const Kind*> FulfilledPartialKindState::make_triggered_counterparts(
    MethodContext* context,
    const PartialKind* unfulfilled_kind) const {
  std::vector<const Kind*> result;
  for (const auto& [kind, rules_map] : map_) {
    if (unfulfilled_kind->is_counterpart(kind)) {
      for (const auto& [rule, _features] : rules_map) {
        result.emplace_back(
            context->kinds.get_triggered(unfulfilled_kind, rule));
      }
    }
  }

  // Empty if the unfulfilled kind does not have a counterpart that is
  // fulfilled.
  return result;
}

void FulfilledPartialKindState::add_fulfilled_kind(
    const PartialKind* kind,
    const MultiSourceMultiSinkRule* rule,
    const FeatureMayAlwaysSet& features) {
  auto rules_map = map_.find(kind);
  if (rules_map == map_.end()) {
    map_.emplace(kind, RuleMap{{rule, features}});
    return;
  }
  rules_map->second.emplace(rule, features);
}

void FulfilledPartialKindState::erase(
    const PartialKind* kind,
    const MultiSourceMultiSinkRule* rule) {
  auto rules_map = map_.find(kind);
  if (rules_map == map_.end()) {
    return;
  }

  rules_map->second.erase(rule);
  if (rules_map->second.empty()) {
    map_.erase(kind);
  }
}

} // namespace marianatrench

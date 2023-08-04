/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/FulfilledPartialKindState.h>
#include <optional>

namespace marianatrench {

bool FulfilledPartialKindState::empty() const {
  return map_.empty();
}

std::optional<Taint> FulfilledPartialKindState::fulfill_kind(
    const PartialKind* kind,
    const MultiSourceMultiSinkRule* rule,
    const FeatureMayAlwaysSet& features,
    const Taint& sink,
    const KindFactory& kind_factory) {
  const auto* counterpart = get_fulfilled_counterpart(kind, rule);
  if (counterpart != nullptr) {
    // If both partial sinks for the callsite have been fulfilled, the rule
    // is satisfied. Make this a triggered sink and create the sink flow taint
    // for the issue. Include the features from both flows (using .add, NOT
    // .join).
    const auto* triggered_kind = kind_factory.get_triggered(kind, rule);
    auto sink_features = get_features(counterpart, rule);
    sink_features.add(features);

    auto issue_sink = sink;
    issue_sink.transform_kind_with_features(
        [kind, triggered_kind](const Kind* sink_kind) {
          // The given taint should only contain the given partial kind.
          // Transform it into the triggered kind.
          mt_assert(sink_kind == kind);
          return std::vector<const Kind*>{triggered_kind};
        },
        [&sink_features](const Kind* /* new_sink_kind */) {
          return sink_features;
        });
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
    const PartialKind* unfulfilled_kind,
    const KindFactory& kind_factory) const {
  std::vector<const Kind*> result;
  for (const auto& [kind, rules_map] : map_) {
    if (unfulfilled_kind->is_counterpart(kind)) {
      for (const auto& [rule, _features] : rules_map) {
        result.emplace_back(kind_factory.get_triggered(unfulfilled_kind, rule));
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

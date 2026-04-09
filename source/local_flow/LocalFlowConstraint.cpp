/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/local_flow/LocalFlowConstraint.h>

#include <algorithm>

namespace marianatrench {
namespace local_flow {

bool LocalFlowConstraint::operator==(const LocalFlowConstraint& other) const {
  return source == other.source && target == other.target &&
      labels == other.labels && guard == other.guard;
}

bool LocalFlowConstraint::operator<(const LocalFlowConstraint& other) const {
  if (source != other.source) {
    return source < other.source;
  }
  if (target != other.target) {
    return target < other.target;
  }
  if (labels != other.labels) {
    return labels < other.labels;
  }
  return guard < other.guard;
}

std::size_t LocalFlowConstraint::structural_depth() const {
  return std::count_if(labels.begin(), labels.end(), [](const Label& l) {
    return l.is_structural();
  });
}

// Debug serialization of raw constraints. The final output format is produced
// by LocalFlowResult::to_json() which applies reverse_guard() transformation.
Json::Value LocalFlowConstraint::to_json() const {
  auto result = Json::Value(Json::objectValue);
  result["source"] = source.to_json();

  auto labels_json = Json::Value(Json::arrayValue);
  for (const auto& label : labels) {
    labels_json.append(label.to_json());
  }
  result["labels"] = labels_json;

  result["target"] = target.to_json();
  if (guard == Guard::Bwd) {
    result["guard"] = "Bwd";
  }
  return result;
}

void LocalFlowConstraintSet::add(LocalFlowConstraint constraint) {
  constraints_.push_back(std::move(constraint));
}

void LocalFlowConstraintSet::add_edge(
    LocalFlowNode source,
    LocalFlowNode target) {
  constraints_.push_back(
      LocalFlowConstraint{std::move(source), {}, std::move(target)});
}

void LocalFlowConstraintSet::add_edge(
    LocalFlowNode source,
    LabelPath labels,
    LocalFlowNode target) {
  constraints_.push_back(
      LocalFlowConstraint{
          std::move(source), std::move(labels), std::move(target)});
}

const std::vector<LocalFlowConstraint>& LocalFlowConstraintSet::constraints()
    const {
  return constraints_;
}

std::size_t LocalFlowConstraintSet::size() const {
  return constraints_.size();
}

bool LocalFlowConstraintSet::empty() const {
  return constraints_.empty();
}

void LocalFlowConstraintSet::remove_temp_only() {
  constraints_.erase(
      std::remove_if(
          constraints_.begin(),
          constraints_.end(),
          [](const LocalFlowConstraint& c) {
            return c.source.is_temp() && c.target.is_temp();
          }),
      constraints_.end());
}

void LocalFlowConstraintSet::deduplicate() {
  std::sort(constraints_.begin(), constraints_.end());
  constraints_.erase(
      std::unique(constraints_.begin(), constraints_.end()),
      constraints_.end());
}

Json::Value LocalFlowConstraintSet::to_json() const {
  auto result = Json::Value(Json::arrayValue);
  for (const auto& constraint : constraints_) {
    result.append(constraint.to_json());
  }
  return result;
}

} // namespace local_flow
} // namespace marianatrench

/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/functional/hash.hpp>

#include <Show.h>

#include <mariana-trench/Issue.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

bool Issue::leq(const Issue& other) const {
  if (is_bottom()) {
    return true;
  } else if (other.is_bottom()) {
    return false;
  } else {
    return rule_ == other.rule_ && position_ == other.position_ &&
        sources_.leq(other.sources_) && sinks_.leq(other.sinks_);
  }
}

bool Issue::equals(const Issue& other) const {
  if (is_bottom()) {
    return other.is_bottom();
  } else if (other.is_bottom()) {
    return false;
  } else {
    return rule_ == other.rule_ && position_ == other.position_ &&
        sources_ == other.sources_ && sinks_ == other.sinks_;
  }
}

void Issue::join_with(const Issue& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    *this = other;
  } else if (other.is_bottom()) {
    return;
  } else {
    mt_assert(rule_ == other.rule_);
    mt_assert(position_ == other.position_);

    sources_.join_with(other.sources_);
    sinks_.join_with(other.sinks_);
  }

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void Issue::widen_with(const Issue& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    *this = other;
  } else if (other.is_bottom()) {
    return;
  } else {
    mt_assert(rule_ == other.rule_);
    mt_assert(position_ == other.position_);

    sources_.widen_with(other.sources_);
    sinks_.widen_with(other.sinks_);
  }

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void Issue::meet_with(const Issue& other) {
  if (is_bottom()) {
    return;
  } else if (other.is_bottom()) {
    set_to_bottom();
  } else {
    mt_assert(rule_ == other.rule_);
    mt_assert(position_ == other.position_);

    sources_.meet_with(other.sources_);
    sinks_.meet_with(other.sinks_);
  }
}

void Issue::narrow_with(const Issue& other) {
  if (is_bottom()) {
    return;
  } else if (other.is_bottom()) {
    set_to_bottom();
  } else {
    mt_assert(rule_ == other.rule_);
    mt_assert(position_ == other.position_);

    sources_.narrow_with(other.sources_);
    sinks_.narrow_with(other.sinks_);
  }
}

bool Issue::GroupEqual::operator()(const Issue& left, const Issue& right)
    const {
  return left.rule_ == right.rule_ && left.position_ == right.position_;
}

std::size_t Issue::GroupHash::operator()(const Issue& issue) const {
  std::size_t seed = 0;
  boost::hash_combine(seed, issue.rule_);
  boost::hash_combine(seed, issue.position_);
  return seed;
}

void Issue::filter_sources(const std::function<bool(const Frame&)>& predicate) {
  sources_.map([&](FrameSet& frames) { frames.filter(predicate); });
}

void Issue::filter_sinks(const std::function<bool(const Frame&)>& predicate) {
  sinks_.map([&](FrameSet& frames) { frames.filter(predicate); });
}

FeatureMayAlwaysSet Issue::features() const {
  auto source_features = FeatureMayAlwaysSet::bottom();
  for (const auto& frames : sources_) {
    source_features.join_with(frames.features_joined());
  }

  auto sink_features = FeatureMayAlwaysSet::bottom();
  for (const auto& frames : sinks_) {
    sink_features.join_with(frames.features_joined());
  }

  source_features.add(sink_features);
  return source_features;
}

Json::Value Issue::to_json() const {
  mt_assert(!is_bottom());

  auto value = Json::Value(Json::objectValue);
  value["sources"] = sources_.to_json();
  value["sinks"] = sinks_.to_json();
  value["rule"] = Json::Value(rule_->code());
  value["position"] = position_->to_json();
  JsonValidation::update_object(value, features().to_json());

  return value;
}

std::ostream& operator<<(std::ostream& out, const Issue& issue) {
  out << "Issue(sources=" << issue.sources_ << ", sinks=" << issue.sinks_
      << ", rule=";
  if (issue.rule_ != nullptr) {
    out << issue.rule_->code();
  } else {
    out << "null";
  }
  return out << ", position=" << show(issue.position_) << ")";
}

} // namespace marianatrench

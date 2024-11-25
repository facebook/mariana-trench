/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/AbstractTaintTree.h>

namespace marianatrench {

bool AbstractTaintTree::is_bottom() const {
  return taint_.is_bottom() && aliases_.is_bottom();
}

bool AbstractTaintTree::is_top() const {
  return taint_.is_top() && aliases_.is_top();
}

bool AbstractTaintTree::leq(const AbstractTaintTree& other) const {
  return taint_.leq(other.taint_) && aliases_.leq(other.aliases_);
}

bool AbstractTaintTree::equals(const AbstractTaintTree& other) const {
  return taint_.equals(other.taint_) && aliases_.equals(other.aliases_);
}

void AbstractTaintTree::set_to_bottom() {
  taint_.set_to_bottom();
  aliases_.set_to_bottom();
}

void AbstractTaintTree::set_to_top() {
  taint_.set_to_top();
  aliases_.set_to_top();
}

void AbstractTaintTree::join_with(const AbstractTaintTree& other) {
  mt_if_expensive_assert(auto previous = *this);

  taint_.join_with(other.taint_);
  aliases_.join_with(other.aliases_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void AbstractTaintTree::widen_with(const AbstractTaintTree& other) {
  mt_if_expensive_assert(auto previous = *this);

  taint_.widen_with(other.taint_);
  aliases_.widen_with(other.aliases_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void AbstractTaintTree::meet_with(const AbstractTaintTree& other) {
  taint_.meet_with(other.taint_);
  aliases_.meet_with(other.aliases_);
}

void AbstractTaintTree::narrow_with(const AbstractTaintTree& other) {
  taint_.narrow_with(other.taint_);
  aliases_.narrow_with(other.aliases_);
}

void AbstractTaintTree::write(
    const Path& path,
    AbstractTaintTree tree,
    UpdateKind kind) {
  taint_.write(path, std::move(tree.taint_), kind);
  aliases_.write(path, std::move(tree.aliases_), kind);
}

void AbstractTaintTree::add_local_position(const Position* position) {
  if (position == nullptr) {
    return;
  }

  taint_.transform([position](Taint taint) {
    taint.add_local_position(position);
    return taint;
  });

  aliases_.transform([position](PointsToSet points_to) {
    points_to.add_local_position(position);
    return points_to;
  });
}

void AbstractTaintTree::add_locally_inferred_features(
    const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  taint_.transform([&features](Taint taint) {
    taint.add_locally_inferred_features(features);
    return taint;
  });

  aliases_.transform([&features](PointsToSet points_to) {
    points_to.add_locally_inferred_features(features);
    return points_to;
  });
}

std::ostream& operator<<(std::ostream& out, const AbstractTaintTree& tree) {
  out << "AbstractTaintTree(\n"
      << "    taint={" << tree.taint_ << "},\n"
      << "    aliases={" << tree.aliases_ << "}";
  return out << ")";
}

} // namespace marianatrench

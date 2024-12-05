/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>

#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/TaintTree.h>

namespace marianatrench {

namespace {

std::optional<std::size_t> calculate_override(
    const TaintTreeConfigurationOverrides& global_config_overrides,
    const TaintTreeConfigurationOverrides& config_overrides,
    TaintTreeConfigurationOverrideOptions option) {
  if (global_config_overrides.is_bottom() && config_overrides.is_bottom()) {
    return std::nullopt;
  }

  return static_cast<std::size_t>(std::max(
      global_config_overrides.get(option), config_overrides.get(option)));
}

} // namespace

Taint TaintTreeConfiguration::transform_on_widening_collapse(Taint taint) {
  // Add the feature as a may-feature, otherwise we would break the widening
  // invariant: a <= a widen b.
  // Use `FeatureFactory::singleton()` since we have no other way to get the
  // current context.
  taint.add_locally_inferred_features(FeatureMayAlwaysSet::make_may(
      {FeatureFactory::singleton().get_widen_broadening_feature()}));
  taint.update_maximum_collapse_depth(CollapseDepth::zero());
  return taint;
}

bool TaintTree::is_bottom() const {
  return tree_.is_bottom() && overrides_.is_bottom();
}

bool TaintTree::is_top() const {
  return tree_.is_top() && overrides_.is_top();
}

bool TaintTree::leq(const TaintTree& other) const {
  return tree_.leq(other.tree_) && overrides_.leq(other.overrides_);
}

bool TaintTree::equals(const TaintTree& other) const {
  return tree_.equals(other.tree_) && overrides_.equals(other.overrides_);
}

void TaintTree::set_to_bottom() {
  tree_.set_to_bottom();
  overrides_.set_to_bottom();
}

void TaintTree::set_to_top() {
  tree_.set_to_top();
  overrides_.set_to_top();
}

void TaintTree::join_with(const TaintTree& other) {
  mt_if_expensive_assert(auto previous = *this);

  tree_.join_with(other.tree_);
  overrides_.join_with(other.overrides_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void TaintTree::widen_with(const TaintTree& other) {
  mt_if_expensive_assert(auto previous = *this);

  tree_.widen_with(other.tree_);
  overrides_.widen_with(other.overrides_);

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void TaintTree::meet_with(const TaintTree& other) {
  tree_.meet_with(other.tree_);
  overrides_.meet_with(other.overrides_);
}

void TaintTree::narrow_with(const TaintTree& other) {
  tree_.narrow_with(other.tree_);
  overrides_.narrow_with(other.overrides_);
}

TaintTree TaintTree::read(const Path& path) const {
  return TaintTree(tree_.read(path), overrides_);
}

TaintTree TaintTree::raw_read(const Path& path) const {
  return TaintTree(tree_.raw_read(path), overrides_);
}

void TaintTree::write(const Path& path, Taint taint, UpdateKind kind) {
  tree_.write(path, std::move(taint), kind);
}

void TaintTree::write(const Path& path, TaintTree tree, UpdateKind kind) {
  tree_.write(path, std::move(tree.tree_), kind);
  switch (kind) {
    case UpdateKind::Strong: {
      overrides_ = tree.overrides_;
    } break;

    case UpdateKind::Weak: {
      overrides_.join_with(std::move(tree.overrides_));
    } break;
  }
}

std::vector<std::pair<Path, const Taint&>> TaintTree::elements() const {
  return tree_.elements();
}

void TaintTree::add_locally_inferred_features(
    const FeatureMayAlwaysSet& features) {
  if (features.empty()) {
    return;
  }

  tree_.transform([&features](Taint taint) {
    taint.add_locally_inferred_features(features);
    return taint;
  });
}

void TaintTree::add_local_position(const Position* position) {
  tree_.transform([position](Taint taint) {
    taint.add_local_position(position);
    return taint;
  });
}

void TaintTree::add_locally_inferred_features_and_local_position(
    const FeatureMayAlwaysSet& features,
    const Position* MT_NULLABLE position) {
  if (features.empty() && position == nullptr) {
    return;
  }

  tree_.transform([&features, position](Taint taint) {
    taint.add_locally_inferred_features_and_local_position(features, position);
    return taint;
  });
}

void TaintTree::attach_position(const Position* position) {
  tree_.transform(
      [position](Taint taint) { return taint.attach_position(position); });
}

Taint TaintTree::collapse(
    const FeatureMayAlwaysSet& broadening_features) const {
  return tree_.collapse([&broadening_features](Taint taint) {
    taint.add_locally_inferred_features(broadening_features);
    taint.update_maximum_collapse_depth(CollapseDepth::zero());
    return taint;
  });
}

Taint TaintTree::collapse() const {
  return tree_.collapse();
}

void TaintTree::collapse_deeper_than(
    std::size_t height,
    const FeatureMayAlwaysSet& broadening_features) {
  tree_.collapse_deeper_than(height, [&broadening_features](Taint taint) {
    taint.add_locally_inferred_features(broadening_features);
    taint.update_maximum_collapse_depth(CollapseDepth::zero());
    return taint;
  });
}

void TaintTree::limit_leaves(
    std::size_t default_max_leaves,
    const FeatureMayAlwaysSet& broadening_features) {
  limit_leaves(
      default_max_leaves,
      TaintTreeConfigurationOverrides::bottom(),
      broadening_features);
}

void TaintTree::limit_leaves(
    std::size_t default_max_leaves,
    const TaintTreeConfigurationOverrides& global_config_overrides,
    const FeatureMayAlwaysSet& broadening_features) {
  // Select the override to use (if any)
  auto override_max_leaves = calculate_override(
      global_config_overrides,
      overrides_,
      TaintTreeConfigurationOverrideOptions::MaxModelWidth);
  auto max_leaves =
      override_max_leaves ? *override_max_leaves : default_max_leaves;

  // Update the override options if it is different from the default heuristic.
  if (max_leaves != default_max_leaves) {
    overrides_.add(
        TaintTreeConfigurationOverrideOptions::MaxModelWidth, max_leaves);
  }

  // Limit the number of leaves on the tree to the selected max_leaves.
  tree_.limit_leaves(max_leaves, [&broadening_features](Taint taint) {
    taint.add_locally_inferred_features(broadening_features);
    taint.update_maximum_collapse_depth(CollapseDepth::zero());
    return taint;
  });
}

void TaintTree::update_maximum_collapse_depth(CollapseDepth collapse_depth) {
  tree_.transform([collapse_depth](Taint taint) {
    taint.update_maximum_collapse_depth(collapse_depth);
    return taint;
  });
}

void TaintTree::update_with_propagation_trace(
    const CallInfo& propagation_call_info,
    const Frame& propagation_frame) {
  tree_.transform(
      [&propagation_call_info, &propagation_frame](const Taint& taint) {
        return taint.update_with_propagation_trace(
            propagation_call_info, propagation_frame);
      });
}

void TaintTree::apply_config_overrides(
    const TaintTreeConfigurationOverrides& config_overrides) {
  overrides_.join_with(config_overrides);
}

void TaintTree::apply_aliasing_properties(
    const AliasingProperties& properties) {
  if (properties.is_bottom() || properties.is_empty()) {
    return;
  }

  tree_.transform([&properties](Taint taint) {
    taint.add_locally_inferred_features(properties.locally_inferred_features());
    taint.add_local_positions(properties.local_positions());

    return taint;
  });
}

} // namespace marianatrench

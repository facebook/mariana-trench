/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/TaintTree.h>

namespace marianatrench {

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

TaintTree TaintTree::read(const Path& path) const {
  return TaintTree(tree_.read(path));
}

TaintTree TaintTree::raw_read(const Path& path) const {
  return TaintTree(tree_.raw_read(path));
}

void TaintTree::write(const Path& path, Taint taint, UpdateKind kind) {
  tree_.write(path, std::move(taint), kind);
}

void TaintTree::write(const Path& path, TaintTree tree, UpdateKind kind) {
  tree_.write(path, std::move(tree.tree_), kind);
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
    std::size_t max_leaves,
    const FeatureMayAlwaysSet& broadening_features) {
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

} // namespace marianatrench

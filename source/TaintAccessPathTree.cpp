/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/TaintAccessPathTree.h>

namespace marianatrench {

TaintTree TaintAccessPathTree::read(Root root) const {
  return TaintTree(tree_.read(root));
}

TaintTree TaintAccessPathTree::read(const AccessPath& access_path) const {
  return TaintTree(tree_.read(access_path));
}

TaintTree TaintAccessPathTree::raw_read(const AccessPath& access_path) const {
  return TaintTree(tree_.raw_read(access_path));
}

void TaintAccessPathTree::write(
    const AccessPath& access_path,
    Taint taint,
    UpdateKind kind) {
  tree_.write(access_path, std::move(taint), kind);
}

void TaintAccessPathTree::write(
    const AccessPath& access_path,
    TaintTree tree,
    UpdateKind kind) {
  tree_.write(access_path, std::move(tree.tree_), kind);
}

std::vector<std::pair<AccessPath, const Taint&>> TaintAccessPathTree::elements()
    const {
  return tree_.elements();
}

std::vector<std::pair<Root, TaintTree>> TaintAccessPathTree::roots() const {
  // Since sizeof(TaintTree) != sizeof(AbstractTreeDomain<Taint>), we cannot
  // return references to `TaintTree`s.
  std::vector<std::pair<Root, TaintTree>> results;
  for (const auto& [root, tree] : tree_) {
    results.emplace_back(root, TaintTree(tree));
  }
  return results;
}

void TaintAccessPathTree::limit_leaves(
    std::size_t max_leaves,
    const FeatureMayAlwaysSet& broadening_features) {
  tree_.limit_leaves(max_leaves, [&broadening_features](Taint taint) {
    taint.add_locally_inferred_features(broadening_features);
    taint.update_maximum_collapse_depth(CollapseDepth::zero());
    return taint;
  });
}

} // namespace marianatrench

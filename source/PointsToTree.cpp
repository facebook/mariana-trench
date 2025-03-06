/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/PointsToTree.h>

namespace marianatrench {

PointsToTree PointsToTree::raw_read(const Path& path) const {
  return PointsToTree{tree_.raw_read(path)};
}

std::pair<Path, PointsToTree> PointsToTree::raw_read_max_path(
    const Path& path) const {
  auto [remaining_path, tree] = tree_.raw_read_max_path(path);
  return {remaining_path, PointsToTree(std::move(tree))};
}

void PointsToTree::write(
    const Path& path,
    const PointsToSet& points_to_set,
    UpdateKind kind) {
  tree_.write(path, points_to_set, kind);
}

void PointsToTree::write(const Path& path, PointsToTree tree, UpdateKind kind) {
  tree_.write(path, std::move(tree.tree_), kind);
}

PointsToTree PointsToTree::with_aliasing_properties(
    const AliasingProperties& properties) const {
  auto result = *this;
  result.write(
      Path{},
      tree_.root().with_aliasing_properties(properties),
      UpdateKind::Weak);

  return result;
}

std::ostream& operator<<(std::ostream& out, const PointsToTree& tree) {
  return out << "PointsToTree(tree=" << tree.tree_ << ")";
}

} // namespace marianatrench

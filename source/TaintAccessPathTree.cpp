/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/FeatureFactory.h>
#include <mariana-trench/TaintAccessPathTree.h>
#include <mariana-trench/TaintTree.h>

namespace marianatrench {

TaintAccessPathTree::TaintAccessPathTree(
    std::initializer_list<std::pair<AccessPath, Taint>> edges) {
  for (const auto& [access_path, elements] : edges) {
    write(access_path, elements, UpdateKind::Weak);
  }
}

TaintTree TaintAccessPathTree::read(Root root) const {
  return map_.get(root);
}

TaintTree TaintAccessPathTree::read(const AccessPath& access_path) const {
  return map_.get(access_path.root()).read(access_path.path());
}

TaintTree TaintAccessPathTree::raw_read(const AccessPath& access_path) const {
  return map_.get(access_path.root()).raw_read(access_path.path());
}

void TaintAccessPathTree::write(
    const AccessPath& access_path,
    Taint taint,
    UpdateKind kind) {
  map_.update(
      access_path.root(),
      [&access_path, &taint, kind](const TaintTree& taint_tree) {
        auto copy = taint_tree;
        copy.write(access_path.path(), std::move(taint), kind);
        return copy;
      });
}

void TaintAccessPathTree::write(
    const AccessPath& access_path,
    TaintTree taint_tree,
    UpdateKind kind) {
  map_.update(
      access_path.root(),
      [&access_path, &taint_tree, kind](const TaintTree& subtree) {
        auto copy = subtree;
        copy.write(access_path.path(), std::move(taint_tree), kind);
        return copy;
      });
}

std::vector<std::pair<AccessPath, const Taint&>> TaintAccessPathTree::elements()
    const {
  std::vector<std::pair<AccessPath, const Taint&>> results;
  visit([&results](const AccessPath& access_path, const Taint& element) {
    results.push_back({access_path, element});
  });
  return results;
}

std::vector<std::pair<Root, const TaintTree&>> TaintAccessPathTree::roots()
    const {
  std::vector<std::pair<Root, const TaintTree&>> results;
  for (const auto& [root, taint_tree] : map_) {
    results.emplace_back(root, taint_tree);
  }
  return results;
}

void TaintAccessPathTree::limit_leaves(
    std::size_t max_leaves,
    const FeatureMayAlwaysSet& broadening_features) {
  map_.transform([max_leaves, &broadening_features](TaintTree taint_tree) {
    taint_tree.limit_leaves(max_leaves, broadening_features);
    return taint_tree;
  });
}

std::ostream& operator<<(std::ostream& out, const TaintAccessPathTree& tree) {
  out << "TaintAccessPathTree {";
  for (auto iterator = tree.map_.begin(), end = tree.map_.end();
       iterator != end;) {
    out << iterator->first << " -> " << iterator->second;
    ++iterator;
    if (iterator != end) {
      out << ", ";
    }
  }
  return out << "}";
}

} // namespace marianatrench

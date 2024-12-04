/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sparta/PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/AbstractTreeDomain.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/MemoryLocation.h>
#include <mariana-trench/MemoryLocationEnvironment.h>
#include <mariana-trench/PointsToSet.h>

namespace marianatrench {

struct PointsToTreeConfiguration {
  static std::size_t max_tree_height_after_widening() {
    // Using the Heuristics::propagation_output_path_tree_widening_height, i.e.
    // the maximum height of the output path tree of propagations after
    // widening as this will be the maximum level of aliasing we will be able to
    // track through propagations.
    return Heuristics::singleton()
        .propagation_output_path_tree_widening_height();
  }

  static PointsToSet transform_on_widening_collapse(
      PointsToSet /* points_to_set */) {
    mt_unreachable(); // Not implemented.
  }

  static PointsToSet transform_on_sink(PointsToSet /* points_to_set */) {
    // PointsToTree does not propagate the points-to set down to children.
    return PointsToSet::bottom();
  }

  static PointsToSet transform_on_hoist(PointsToSet /* points_to_set */) {
    mt_unreachable(); // Not implemented.
  }
};

class PointsToTree final : public sparta::AbstractDomain<PointsToTree> {
 private:
  using Tree = AbstractTreeDomain<PointsToSet, PointsToTreeConfiguration>;

 public:
  /* Create the bottom (empty) points-to tree. */
  PointsToTree() : tree_(Tree::bottom()) {}

 private:
  explicit PointsToTree(Tree tree) : tree_(std::move(tree)) {}

 public:
  explicit PointsToTree(PointsToSet points_to_set)
      : tree_(std::move(points_to_set)) {}

  explicit PointsToTree(
      std::initializer_list<std::pair<Path, PointsToSet>> edges)
      : tree_(Tree(edges)) {}

  INCLUDE_ABSTRACT_DOMAIN_METHODS(PointsToTree, Tree, tree_)

 public:
  const PointsToSet& root() const {
    return tree_.root();
  }

  const Tree::Map& successors() const {
    return tree_.successors();
  }

  /**
   * Return the points-to set at the given path wrapped as a PointsToTree.
   *
   * Points-to set are _not_ propagated down to children.
   */
  PointsToTree raw_read(const Path& path) const;

  std::pair<Path, PointsToTree> raw_read_max_path(const Path& path) const;

  /* Write the given points-to set at the given path. */
  void
  write(const Path& path, const PointsToSet& points_to_set, UpdateKind kind);

  /* Write the given points-to tree at the given path. */
  void write(const Path& path, PointsToTree tree, UpdateKind kind);

  /**
   * Iterate on all non-empty points-to set in the tree.
   *
   * When visiting the tree, points-to set do not include their ancestors.
   */
  template <typename Visitor> // void(const Path&, const PointsToSet&)
  void visit(Visitor&& visitor) const {
    return tree_.visit(std::forward<Visitor>(visitor));
  }

  /* Apply the given function on all PointsToSet. */
  template <typename Function> // PointsToSet(PointsToSet)
  void transform(Function&& f) {
    tree_.transform(std::forward<Function>(f));
  }

  friend std::ostream& operator<<(std::ostream& out, const PointsToTree& tree);

 private:
  Tree tree_;
};

} // namespace marianatrench

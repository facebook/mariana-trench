/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/AbstractTreeDomain.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Taint.h>
#include <mariana-trench/TaintTree.h>

namespace marianatrench {

class TaintAccessPathTree final
    : public sparta::AbstractDomain<TaintAccessPathTree> {
 private:
  using Map = RootPatriciaTreeAbstractPartition<TaintTree>;

  explicit TaintAccessPathTree(Map map) : map_(std::move(map)) {}

 public:
  /* Create the bottom (empty) taint access path tree. */
  TaintAccessPathTree() : map_(Map::bottom()) {}

  explicit TaintAccessPathTree(
      std::initializer_list<std::pair<AccessPath, Taint>> edges);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(TaintAccessPathTree)

  INCLUDE_ABSTRACT_DOMAIN_METHODS(TaintAccessPathTree, Map, map_)

  /**
   * Apply the config_overrides to the Root of the TaintTree.
   */
  void apply_config_overrides(
      const TaintTreeConfigurationOverrides& config_overrides);

  const TaintTreeConfigurationOverrides& config_overrides(Root root) const;

  TaintTree read(Root root) const;

  template <typename Propagate> // Taint(Taint, Path::Element)
  TaintTree read(const AccessPath& access_path, Propagate&& propagate) const {
    map_.get(access_path.root())
        .read(access_path.path(), std::forward<Propagate>(propagate));
  }

  /* Return the subtree at the given access path. */
  TaintTree read(const AccessPath& access_path) const;

  /**
   * Return the subtree at the given path.
   *
   * Elements are NOT propagated down to children.
   */
  TaintTree raw_read(const AccessPath& access_path) const;

  /* Write taint at the given access path. */
  void write(const AccessPath& access_path, Taint taint, UpdateKind kind);

  /* Write a taint tree at the given access path. */
  void write(const AccessPath& access_path, TaintTree tree, UpdateKind kind);

  /**
   * Iterate on all non-empty taint in the tree.
   *
   * When visiting the tree, taint do not include their ancestors.
   */
  template <typename Visitor> // void(const AccessPath&, const Taint&)
  void visit(Visitor&& visitor) const {
    static_assert(
        std::is_same_v<
            decltype(visitor(
                std::declval<const AccessPath>(), std::declval<const Taint>())),
            void>);
    mt_assert(!is_top());

    for (const auto& [root, taint_tree] : map_) {
      auto access_path = AccessPath(root);
      visit_internal(
          access_path, taint_tree.tree_, std::forward<Visitor>(visitor));
    }
  }

 private:
  template <typename Visitor> // void(const AccessPath&, const TaintTree&)
  static void visit_internal(
      AccessPath& access_path,
      const TaintTree::Tree& tree,
      Visitor&& visitor) {
    if (!tree.root().is_bottom()) {
      visitor(access_path, tree.root());
    }

    for (const auto& [path_element, subtree] : tree.successors()) {
      access_path.append(path_element);
      visit_internal(access_path, subtree, visitor);
      access_path.pop_back();
    }
  }

 public:
  /* Return the list of pairs (access path, taint) in the tree. */
  std::vector<std::pair<AccessPath, const Taint&>> elements() const;

  /* Return the list of pairs (root, taint tree) in the tree. */
  std::vector<std::pair<Root, const TaintTree&>> roots() const;

  /* Apply the given function on all taint. */
  template <typename Function> // Taint(Taint)
  void transform(Function&& f) {
    static_assert(std::is_same_v<decltype(f(std::declval<Taint&&>())), Taint>);

    map_.transform([f = std::forward<Function>(f)](TaintTree tree) {
      tree.transform(f);
      return tree;
    });
  }

  /* Collapse children that have more than `max_leaves` leaves. */
  void limit_leaves(
      std::size_t max_leaves,
      const FeatureMayAlwaysSet& broadening_features);

  /**
   * When a path is invalid, collapse its taint into its parent's.
   * See AbstractTreeDomain::collapse_invalid_paths.
   */
  template <typename Accumulator>
  void collapse_invalid_paths(
      const std::function<
          std::pair<bool, Accumulator>(const Accumulator&, Path::Element)>&
          is_valid,
      const std::function<Accumulator(const Root&)>& initial_accumulator,
      const FeatureMayAlwaysSet& broadening_features) {
    Map new_map;
    for (const auto& [root, taint_tree] : map_) {
      auto copy = taint_tree.tree_;
      copy.collapse_invalid_paths(
          is_valid,
          /* accumulator */ initial_accumulator(root),
          /* transform_on_collapse */ [&broadening_features](Taint taint) {
            taint.add_locally_inferred_features(broadening_features);
            taint.update_maximum_collapse_depth(CollapseDepth::zero());
            return taint;
          });
      new_map.set(root, TaintTree(std::move(copy)));
    }

    map_ = new_map;
  }

  /**
   * Transforms the tree to shape it according to a mold.
   *
   * `make_mold` is a function applied on taint to create a mold tree.
   *
   * This is used to prune the taint tree of duplicate taint, for
   * better performance at the cost of precision. `make_mold` creates a new
   * taint without any non-essential information (i.e, removing features). Since
   * the tree domain automatically removes taint on children if it is present
   * at the root (closure), this will collapse unnecessary branches.
   * `AbstractTreeDomain::shape_with` will then collapse branches in the
   * original taint tree if it was collapsed in the mold.
   */
  template <typename MakeMold>
  void shape_with(
      MakeMold&& make_mold, // Taint(Taint)
      const FeatureMayAlwaysSet& broadening_features) {
    static_assert(
        std::is_same_v<decltype(make_mold(std::declval<Taint&&>())), Taint>);

    map_.transform([make_mold = std::forward<MakeMold>(make_mold),
                    &broadening_features](const TaintTree& taint_tree) {
      auto mold = taint_tree.tree_;
      mold.transform(make_mold);

      auto copy = taint_tree.tree_;
      copy.shape_with(mold, [&broadening_features](Taint taint) {
        taint.add_locally_inferred_features(broadening_features);
        taint.update_maximum_collapse_depth(CollapseDepth::zero());
        return taint;
      });
      return TaintTree(std::move(copy));
    });
  }

  friend std::ostream& operator<<(
      std::ostream& out,
      const TaintAccessPathTree& tree);

 private:
  Map map_;
};

} // namespace marianatrench

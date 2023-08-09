/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/AbstractTreeDomain.h>
#include <mariana-trench/AccessPathTreeDomain.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Taint.h>

namespace marianatrench {

struct TaintTreeConfiguration {
  static std::size_t max_tree_height_after_widening() {
    return Heuristics::kSourceSinkTreeWideningHeight;
  }

  static Taint transform_on_widening_collapse(Taint);

  static Taint transform_on_sink(Taint taint) {
    return taint;
  }

  static Taint transform_on_hoist(Taint taint) {
    return taint;
  }
};

class TaintTree final : public sparta::AbstractDomain<TaintTree> {
 private:
  // We wrap `AbstractTreeDomain<Taint, TaintTreeConfiguration>`
  // in order to properly update the collapse depth when collapsing.

  using Tree = AbstractTreeDomain<Taint, TaintTreeConfiguration>;

  explicit TaintTree(Tree tree) : tree_(std::move(tree)) {}

 public:
  /* Create the bottom (empty) taint tree. */
  TaintTree() : tree_(Tree::bottom()) {}

  explicit TaintTree(Taint taint) : tree_(std::move(taint)) {}

  explicit TaintTree(std::initializer_list<std::pair<Path, Taint>> edges)
      : tree_(std::move(edges)) {}

  INCLUDE_ABSTRACT_DOMAIN_METHODS(TaintTree, Tree, tree_)

  const Taint& root() const {
    return tree_.root();
  }

  /**
   * Return the subtree at the given path.
   *
   * `propagate` is a function that is called when propagating taint down to
   * a child. This is usually used to attach the correct access path to
   * backward taint to infer propagations.
   */
  template <typename Propagate> // Taint(Taint, Path::Element)
  TaintTree read(const Path& path, Propagate&& propagate) const {
    return TaintTree(tree_.read(path, std::forward<Propagate>(propagate)));
  }

  /**
   * Return the subtree at the given path.
   *
   * Taint are propagated down to children unchanged.
   */
  TaintTree read(const Path& path) const;

  /**
   * Return the subtree at the given path.
   *
   * Taint are NOT propagated down to children.
   */
  TaintTree raw_read(const Path& path) const;

  /* Write the given taint at the given path. */
  void write(const Path& path, Taint taint, UpdateKind kind);

  /* Write the given taint tree at the given path. */
  void write(const Path& path, TaintTree tree, UpdateKind kind);

  /**
   * Iterate on all non-empty taint in the tree.
   *
   * When visiting the tree, taint do not include their ancestors.
   */
  template <typename Visitor> // void(const Path&, const Taint&)
  void visit(Visitor&& visitor) const {
    return tree_.visit(std::forward<Visitor>(visitor));
  }

  /* Return the list of all pairs (path, taint) in the tree. */
  std::vector<std::pair<Path, const Taint&>> elements() const;

  void add_locally_inferred_features(const FeatureMayAlwaysSet& features);

  void add_local_position(const Position* position);

  void add_locally_inferred_features_and_local_position(
      const FeatureMayAlwaysSet& features,
      const Position* MT_NULLABLE position);

  void attach_position(const Position* position);

  /* Return all taint in the tree. */
  Taint collapse(const FeatureMayAlwaysSet& broadening_features) const;

  /* Collapse the tree to the given maximum height. */
  void collapse_deeper_than(
      std::size_t height,
      const FeatureMayAlwaysSet& broadening_features);

  /* Collapse children that have more than `max_leaves` leaves. */
  void limit_leaves(
      std::size_t max_leaves,
      const FeatureMayAlwaysSet& broadening_features);

  void update_maximum_collapse_depth(CollapseDepth collapse_depth);

  /* Update the propagation taint tree with the trace information collected from
   * the propagation frame. */
  void update_with_propagation_trace(const Frame& propagation_frame);

  /* Apply the given function on all taint. */
  template <typename Function> // Taint(Taint)
  void map(Function&& f) {
    tree_.map(std::forward<Function>(f));
  }

  friend std::ostream& operator<<(std::ostream& out, const TaintTree& tree) {
    return out << "TaintTree" << tree.tree_;
  }

 private:
  Tree tree_;

 private:
  friend class TaintAccessPathTree;
};

class TaintAccessPathTree final
    : public sparta::AbstractDomain<TaintAccessPathTree> {
 private:
  // We wrap `AccessPathTreeDomain<Taint, TaintTreeConfiguration>`
  // in order to properly update the collapse depth when collapsing.

  using Tree = AccessPathTreeDomain<Taint, TaintTreeConfiguration>;

  explicit TaintAccessPathTree(Tree tree) : tree_(std::move(tree)) {}

 public:
  /* Create the bottom (empty) taint access path tree. */
  TaintAccessPathTree() : tree_(Tree::bottom()) {}

  explicit TaintAccessPathTree(
      std::initializer_list<std::pair<AccessPath, Taint>> edges)
      : tree_(std::move(edges)) {}

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(TaintAccessPathTree)

  INCLUDE_ABSTRACT_DOMAIN_METHODS(TaintAccessPathTree, Tree, tree_)

  TaintTree read(Root root) const;

  template <typename Propagate> // Taint(Taint, Path::Element)
  TaintTree read(const AccessPath& access_path, Propagate&& propagate) const {
    return TaintTree(
        tree_.read(access_path, std::forward<Propagate>(propagate)));
  }

  TaintTree read(const AccessPath& access_path) const;

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
    return tree_.visit(std::forward<Visitor>(visitor));
  }

  /* Return the list of pairs (access path, taint) in the tree. */
  std::vector<std::pair<AccessPath, const Taint&>> elements() const;

  /* Return the list of pairs (root, taint tree) in the tree. */
  std::vector<std::pair<Root, TaintTree>> roots() const;

  /* Apply the given function on all taint. */
  template <typename Function> // Taint(Taint)
  void map(Function&& f) {
    tree_.map(std::forward<Function>(f));
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
    tree_.collapse_invalid_paths(
        is_valid, initial_accumulator, [&broadening_features](Taint taint) {
          taint.add_locally_inferred_features(broadening_features);
          taint.update_maximum_collapse_depth(CollapseDepth::zero());
          return taint;
        });
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
    tree_.shape_with(
        std::forward<MakeMold>(make_mold), [&broadening_features](Taint taint) {
          taint.add_locally_inferred_features(broadening_features);
          taint.update_maximum_collapse_depth(CollapseDepth::zero());
          return taint;
        });
  }

  friend std::ostream& operator<<(
      std::ostream& out,
      const TaintAccessPathTree& tree) {
    return out << "TaintAccessPathTree" << tree.tree_;
  }

 private:
  Tree tree_;
};

} // namespace marianatrench

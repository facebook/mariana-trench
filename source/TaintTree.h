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
#include <mariana-trench/TaintTreeConfigurationOverrides.h>

namespace marianatrench {

struct TaintTreeConfiguration {
  static std::size_t max_tree_height_after_widening() {
    return Heuristics::singleton().source_sink_tree_widening_height();
  }

  static Taint transform_on_widening_collapse(Taint);

  static Taint transform_on_sink(Taint taint) {
    return taint;
  }

  static Taint transform_on_hoist(Taint taint) {
    return taint;
  }
};

// We cannot use `sparta::DirectProductAbstractDomain` because both
// AbstractTreeDomain and TaintTreeConfigurationOverrides are bottom value
// interfaces (i.e. empty is bottom). So, the product domain is never updated.
class TaintTree final : public sparta::AbstractDomain<TaintTree> {
 private:
  // We wrap `AbstractTreeDomain<Taint, TaintTreeConfiguration>`
  // in order to properly update the collapse depth when collapsing.

  using Tree = AbstractTreeDomain<Taint, TaintTreeConfiguration>;

  explicit TaintTree(
      Tree tree,
      TaintTreeConfigurationOverrides config_overrides)
      : tree_(std::move(tree)), overrides_(std::move(config_overrides)) {}

 public:
  /* Create the bottom (empty) taint tree. */
  TaintTree()
      : tree_(Tree::bottom()),
        overrides_(TaintTreeConfigurationOverrides::bottom()) {}

  explicit TaintTree(
      Taint taint,
      TaintTreeConfigurationOverrides config_overrides =
          TaintTreeConfigurationOverrides::bottom())
      : tree_(Tree(std::move(taint))),
        overrides_(std::move(config_overrides)) {}

  explicit TaintTree(
      std::initializer_list<std::pair<Path, Taint>> edges,
      TaintTreeConfigurationOverrides config_overrides =
          TaintTreeConfigurationOverrides::bottom())
      : tree_(Tree(std::move(edges))),
        overrides_(std::move(config_overrides)) {}

  bool is_bottom() const;

  bool is_top() const;

  bool leq(const TaintTree& other) const;

  bool equals(const TaintTree& other) const;

  void set_to_bottom();

  void set_to_top();

  void join_with(const TaintTree& other);

  void widen_with(const TaintTree& other);

  void meet_with(const TaintTree& other);

  void narrow_with(const TaintTree& other);

 public:
  const Taint& root() const {
    return tree_.root();
  }

  const TaintTreeConfigurationOverrides& config_overrides() const {
    return overrides_;
  }

  /**
   * Return the subtree at the given path with the config_overrides.
   *
   * `propagate` is a function that is called when propagating taint down to
   * a child. This is usually used to attach the correct access path to
   * backward taint to infer propagations.
   */
  template <typename Propagate> // Taint(Taint, Path::Element)
  TaintTree read(const Path& path, Propagate&& propagate) const {
    return TaintTree(
        tree_.read(path, std::forward<Propagate>(propagate)), overrides_);
  }

  /**
   * Return the subtree at the given path with the config_overrides.
   *
   * Taint are propagated down to children unchanged.
   */
  TaintTree read(const Path& path) const;

  /**
   * Return the subtree at the given path with the config_overrides.
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
  Taint collapse() const;

  /* Collapse the tree to the given maximum height. */
  void collapse_deeper_than(
      std::size_t height,
      const FeatureMayAlwaysSet& broadening_features);

  /* Collapse children that have more than `max_leaves` leaves. */
  void limit_leaves(
      std::size_t default_max_leaves,
      const FeatureMayAlwaysSet& broadening_features);

  void limit_leaves(
      std::size_t default_max_leaves,
      const TaintTreeConfigurationOverrides& global_config_overrides,
      const FeatureMayAlwaysSet& broadening_features);

  void update_maximum_collapse_depth(CollapseDepth collapse_depth);

  /* Update the propagation taint tree with the trace information collected from
   * the propagation frame. */
  void update_with_propagation_trace(
      const CallInfo& propagation_call_info,
      const Frame& propagation_frame);

  void apply_config_overrides(
      const TaintTreeConfigurationOverrides& config_overrides);

  /* Apply the given function on all taint. */
  template <typename Function> // Taint(Taint)
  void transform(Function&& f) {
    tree_.transform(std::forward<Function>(f));
  }

  friend std::ostream& operator<<(std::ostream& out, const TaintTree& tree) {
    return out << "TaintTree(tree=" << tree.tree_
               << ", overrides=" << tree.overrides_ << ")";
  }

 private:
  Tree tree_;
  TaintTreeConfigurationOverrides overrides_;

 private:
  friend class TaintAccessPathTree;
};

} // namespace marianatrench

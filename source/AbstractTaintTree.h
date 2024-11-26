/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/PointsToTree.h>
#include <mariana-trench/TaintTree.h>

namespace marianatrench {

/** A wrapper domain to encapsulate both taint and points-to trees. */
class AbstractTaintTree final
    : public sparta::AbstractDomain<AbstractTaintTree> {
 public:
  /* Create the bottom tree */
  AbstractTaintTree()
      : taint_(TaintTree::bottom()), aliases_(PointsToTree::bottom()) {}

  explicit AbstractTaintTree(TaintTree taint)
      : taint_(std::move(taint)), aliases_(PointsToTree::bottom()) {}

  explicit AbstractTaintTree(TaintTree taint, PointsToTree aliases)
      : taint_(std::move(taint)), aliases_(std::move(aliases)) {}

  bool is_bottom() const;

  bool is_top() const;

  bool leq(const AbstractTaintTree& other) const;

  bool equals(const AbstractTaintTree& other) const;

  void set_to_bottom();

  void set_to_top();

  void join_with(const AbstractTaintTree& other);

  void widen_with(const AbstractTaintTree& other);

  void meet_with(const AbstractTaintTree& other);

  void narrow_with(const AbstractTaintTree& other);

  void write(const Path& path, AbstractTaintTree tree, UpdateKind kind);

  void write_taint_tree(const Path& path, TaintTree tree, UpdateKind kind);

  void write_alias_tree(const Path& path, PointsToTree tree, UpdateKind kind);

  const TaintTree& taint() const {
    return taint_;
  }

  const PointsToTree& aliases() const {
    return aliases_;
  }

  void add_local_position(const Position* position);

  void add_locally_inferred_features(const FeatureMayAlwaysSet& features);

  friend std::ostream& operator<<(
      std::ostream& out,
      const AbstractTaintTree& tree);

 private:
  TaintTree taint_;
  PointsToTree aliases_;
};

} // namespace marianatrench

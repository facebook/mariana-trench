/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <functional>
#include <initializer_list>
#include <vector>

#include <AbstractDomain.h>
#include <Show.h>

#include <mariana-trench/AbstractTreeDomain.h>
#include <mariana-trench/Access.h>
#include <mariana-trench/AccessPathTreeDomain.h>
#include <mariana-trench/RootPatriciaTreeAbstractPartition.h>

namespace marianatrench {

/**
 * An access path tree domain.
 *
 * This represents a map from roots to abstract trees.
 *
 * See `AbstractTreeDomain` for more information.
 */
template <typename Elements>
class AccessPathTreeDomain final
    : public sparta::AbstractDomain<AccessPathTreeDomain<Elements>> {
 public:
  using AbstractTreeDomainT = AbstractTreeDomain<Elements>;

 private:
  using Map = RootPatriciaTreeAbstractPartition<AbstractTreeDomainT>;

 public:
  // C++ container concept member types
  using key_type = Root;
  using mapped_type = AbstractTreeDomainT;
  using value_type = std::pair<Root, AbstractTreeDomainT>;
  using iterator = typename Map::iterator;
  using const_iterator = iterator;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using const_reference = const value_type&;
  using const_pointer = const value_type*;

 private:
  explicit AccessPathTreeDomain(Map map) : map_(std::move(map)) {}

 public:
  /* Return the bottom value (i.e, the empty tree). */
  AccessPathTreeDomain() = default;

  explicit AccessPathTreeDomain(
      std::initializer_list<std::pair<AccessPath, Elements>> edges) {
    for (const auto& [access_path, elements] : edges) {
      write(access_path, elements, UpdateKind::Weak);
    }
  }

  explicit AccessPathTreeDomain(
      const std::vector<std::pair<AccessPath, Elements>>& edges) {
    for (const auto& [access_path, elements] : edges) {
      write(access_path, elements, UpdateKind::Weak);
    }
  }

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(AccessPathTreeDomain)

  INCLUDE_ABSTRACT_DOMAIN_METHODS(AccessPathTreeDomain, Map, map_)

  /* Write elements at the given access path. */
  void
  write(const AccessPath& access_path, Elements elements, UpdateKind kind) {
    map_.update(access_path.root(), [&](const AbstractTreeDomainT& tree) {
      auto copy = tree;
      copy.write(access_path.path(), std::move(elements), kind);
      return copy;
    });
  }

  /* Write a tree at the given access path. */
  void write(
      const AccessPath& access_path,
      AbstractTreeDomainT tree,
      UpdateKind kind) {
    map_.update(access_path.root(), [&](const AbstractTreeDomainT& subtree) {
      auto copy = subtree;
      copy.write(access_path.path(), std::move(tree), kind);
      return copy;
    });
  }

  const AbstractTreeDomainT& read(Root root) const {
    return map_.get(root);
  }

  template <typename Propagate>
  AbstractTreeDomainT read(
      const AccessPath& access_path,
      const Propagate& propagate) const {
    return map_.get(access_path.root()).read(access_path.path(), propagate);
  }

  AbstractTreeDomainT read(const AccessPath& access_path) const {
    return map_.get(access_path.root()).read(access_path.path());
  }

  AbstractTreeDomainT raw_read(const AccessPath& access_path) const {
    return map_.get(access_path.root()).raw_read(access_path.path());
  }

  /**
   * Iterate on all non-empty elements in the tree.
   *
   * When visiting the tree, elements do not include their ancestors.
   */
  void visit(
      std::function<void(const AccessPath&, const Elements&)> visitor) const {
    mt_assert(!is_top());

    for (const auto& [root, tree] : map_) {
      auto access_path = AccessPath(root);
      visit_internal(access_path, tree, visitor);
    }
  }

 private:
  static void visit_internal(
      AccessPath& access_path,
      const AbstractTreeDomainT& tree,
      std::function<void(const AccessPath&, const Elements&)>& visitor) {
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
  /**
   * Return the list of pairs (access path, elements) in the tree.
   *
   * Elements are returned by reference.
   * Elements do not contain their ancestors.
   */
  std::vector<std::pair<AccessPath, const Elements&>> elements() const {
    std::vector<std::pair<AccessPath, const Elements&>> results;
    visit([&](const AccessPath& access_path, const Elements& element) {
      results.push_back({access_path, element});
    });
    return results;
  }

  /* Apply the given function on all elements. */
  void map(const std::function<void(Elements&)>& f) {
    map_.map([&](const AbstractTreeDomainT& tree) {
      auto copy = tree;
      copy.map(f);
      return copy;
    });
  }

  /* Return the begin iterator over the pairs (root, tree). */
  iterator begin() const {
    return map_.begin();
  }

  /* Return the end iterator over the pairs (root, tree). */
  iterator end() const {
    return map_.end();
  }

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
      const std::function<void(Elements&)>& transform_on_collapse) {
    Map new_map;
    for (const auto& [root, tree] : map_) {
      auto copy = tree;
      copy.collapse_invalid_paths(
          is_valid,
          /* accumulator */ initial_accumulator(root),
          transform_on_collapse);
      new_map.set(root, std::move(copy));
    }

    map_ = new_map;
  }

  /* Collapse children that have more than `max_leaves` leaves. */
  void limit_leaves(std::size_t max_leaves) {
    map_.map([=](const AbstractTreeDomainT& tree) {
      auto copy = tree;
      copy.limit_leaves(max_leaves);
      return copy;
    });
  }

  /*
   * Collapse children that have more than `max_leaves` leaves.
   *
   * `transform` is a function applied to the `Element`s that are collapsed.
   * Mainly used to add broadening features to collapsed taint
   */
  void limit_leaves(
      std::size_t max_leaves,
      const std::function<void(Elements&)>& transform) {
    map_.map([max_leaves, &transform](const AbstractTreeDomainT& tree) {
      auto copy = tree;
      copy.limit_leaves(max_leaves, transform);
      return copy;
    });
  }

  /**
   * Transforms the tree to shape it according to a mold.
   *
   * `make_mold` is a function applied on elements to create a mold tree.
   *
   * `transform_on_collapse` is called on elements that are collapsed. This is
   * mainly used to attach broadening features to collapsed taint.
   *
   * In practice, this is used to prune the taint tree of duplicate taint, for
   * better performance at the cost of precision. `make_mold` creates a new
   * taint without any non-essential information (i.e, removing features). Since
   * the tree domain automatically removes elements on children if it is present
   * at the root (closure), this will collapse unnecessary branches.
   * `AbstractTreeDomain::shape_with` will then collapse branches in the
   * original taint tree if it was collapsed in the mold.
   */
  void shape_with(
      const std::function<void(Elements&)>& make_mold,
      const std::function<void(Elements&)>& transform_on_collapse) {
    map_.map(
        [&make_mold, &transform_on_collapse](const AbstractTreeDomainT& tree) {
          auto mold = tree;
          mold.map(make_mold);

          auto copy = tree;
          copy.shape_with(mold, transform_on_collapse);
          return copy;
        });
  }

  friend std::ostream& operator<<(
      std::ostream& out,
      const AccessPathTreeDomain& tree) {
    out << "AccessPathTree{";
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

 private:
  Map map_;
};

} // namespace marianatrench

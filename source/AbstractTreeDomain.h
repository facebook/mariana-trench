/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <functional>
#include <initializer_list>
#include <iterator>
#include <vector>

#include <boost/iterator/transform_iterator.hpp>

#include <sparta/AbstractDomain.h>
#include <sparta/PatriciaTreeMap.h>

#include <Show.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/Heuristics.h>
#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

namespace {

template <typename AbstractTreeDomain>
const AbstractTreeDomain& get_element_or_star(
    const typename AbstractTreeDomain::Map& children,
    typename AbstractTreeDomain::PathElement path_element,
    const AbstractTreeDomain& subtree_star) {
  auto& subtree = children.at(path_element);

  if (path_element.is_index() && subtree.is_bottom()) {
    return subtree_star;
  }

  return subtree;
}

} // namespace

enum class UpdateKind {
  /* Perform a strong update, i.e previous elements are replaced. */
  Strong,

  /* Perform a weak update, i.e elements are joined. */
  Weak,
};

/**
 * An abstract tree domain, where edges are access path elements (for instance,
 * fields) and nodes store elements.
 *
 * `Elements` should be a abstract domain.
 *
 * `Configuration` should be a structure containing the following components:
 *
 * ```
 * struct Configuration {
 *   // Maximum tree depth after widening.
 *   static std::size_t max_tree_height_after_widening();
 *
 *   // Transform elements that are collapsed during widening.
 *   static Elements transform_on_widening_collapse(Elements);
 *
 *   // Transform elements implicitly propagated down in the tree.
 *   static Elements transform_on_sink(Elements);
 *
 *   // Transform elements implicitly propagated up in the tree.
 *   static Elements transform_on_hoist(Elements);
 * }
 * ```
 *
 * This is mainly used with a source set or a sink set as `Elements`, to store
 * the taint on each access paths.
 *
 * `Elements` on nodes are implicitly propagated to their children.
 *
 */
template <typename Elements, typename Configuration>
class AbstractTreeDomain final
    : public sparta::AbstractDomain<
          AbstractTreeDomain<Elements, Configuration>> {
 private:
  // Check requirements.
  static_assert(std::is_same_v<
                decltype(Configuration::max_tree_height_after_widening()),
                std::size_t>);
  static_assert(std::is_same_v<
                decltype(Configuration::transform_on_widening_collapse(
                    std::declval<const Elements>())),
                Elements>);
  static_assert(
      std::is_same_v<
          decltype(Configuration::transform_on_sink(std::declval<Elements&>())),
          Elements>);
  static_assert(std::is_same_v<
                decltype(Configuration::transform_on_hoist(
                    std::declval<Elements&>())),
                Elements>);

 public:
  using PathElement = typename Path::Element;

 private:
  struct ValueInterface final
      : public sparta::AbstractMapValue<ValueInterface> {
    using type = AbstractTreeDomain;

    static AbstractTreeDomain default_value() {
      return AbstractTreeDomain::bottom();
    }

    static bool is_default_value(const AbstractTreeDomain& x) {
      return x.is_bottom();
    }

    static bool equals(
        const AbstractTreeDomain& x,
        const AbstractTreeDomain& y) {
      // This is a structural equality, because this is used in
      // `sparta::PatriciaTreeMap`'s implementation to avoid node duplication.
      return x.elements_.equals(y.elements_) &&
          x.children_.reference_equals(y.children_);
    }

    static bool leq(
        const AbstractTreeDomain& /*x*/,
        const AbstractTreeDomain& /*y*/) {
      mt_unreachable(); // Never used.
    }

    constexpr static sparta::AbstractValueKind default_value_kind =
        sparta::AbstractValueKind::Bottom;
  };

 public:
  using Map =
      sparta::PatriciaTreeMap<PathElement, AbstractTreeDomain, ValueInterface>;

 public:
  /* Return the bottom value (i.e, the empty tree). */
  AbstractTreeDomain() : elements_(Elements::bottom()) {}

  explicit AbstractTreeDomain(Elements elements)
      : elements_(std::move(elements)) {}

  explicit AbstractTreeDomain(
      std::initializer_list<std::pair<Path, Elements>> edges)
      : elements_(Elements::bottom()) {
    for (const auto& [path, elements] : edges) {
      write(path, elements, UpdateKind::Weak);
    }
  }

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(AbstractTreeDomain)

  static AbstractTreeDomain bottom() {
    return AbstractTreeDomain();
  }

  static AbstractTreeDomain top() {
    mt_unreachable(); // Not implemented.
  }

  bool is_bottom() const {
    return elements_.is_bottom() && children_.empty();
  }

  bool is_top() const {
    return false;
  }

  void set_to_bottom() {
    elements_.set_to_bottom();
    children_.clear();
  }

  void set_to_top() {
    mt_unreachable(); // Not implemented.
  }

  const Elements& root() const {
    return elements_;
  }

  const Map& successors() const {
    return children_;
  }

  const AbstractTreeDomain& successor(PathElement path_element) const {
    return children_.at(path_element);
  }

  /**
   * Less or equal comparison is tricky because of the special meaning of [*]
   * and [f]. We have to consider the three sets of indices:
   *   L : indices [f] only in left_tree
   *   R : indices [f] only in right_tree
   *   C : indices [f] common in left_tree and right_tree.
   *
   * The result of leq is then:
   *  left_tree.elements <= right.elements /\
   *  left_tree[c] <= right_tree[c] for all c in C /\
   *  left_tree[*] <= right_tree[*] /\
   *  left_tree[*] <= right_tree[r] for all r in R /\
   *  left_tree[l] <= right_tree[*] for all l in L.
   */
  bool leq(const AbstractTreeDomain& other) const {
    // Case: left_tree.elements <= right_tree.elements
    if (!elements_.leq(other.elements_)) {
      return false;
    }

    if (children_.reference_equals(other.children_)) {
      return true;
    }

    const auto& other_subtree_star =
        other.children_.at(PathElement::any_index());

    // Cases:
    //  - left_tree[c] <= right_tree[c] for all c in C
    //  - left_tree[*] <= right_tree[*] if left_tree[*] present
    //  - left_tree[l] <= right_tree[*] for all l in L.
    for (const auto& [path_element, subtree] : children_) {
      // Default to right_tree[*] for set of indices L
      auto other_subtree_copy = get_element_or_star(
          other.children_, path_element, other_subtree_star);

      // Read semantics: we propagate the elements to the children.
      other_subtree_copy.elements_.join_with(other.elements_);

      if (!subtree.leq(other_subtree_copy)) {
        return false;
      }
    }

    const auto& subtree_star = children_.at(PathElement::any_index());

    if (!subtree_star.is_bottom()) {
      // Cases:
      //  - left_tree[*] <= right_tree[r] for all r in R
      //  - left_tree[*] <= right_tree[*] if right_tree[*] present.
      for (const auto& [path_element, other_subtree] : other.children_) {
        if (path_element.is_field()) {
          continue;
        }

        const auto& subtree = children_.at(path_element);

        if (!subtree.is_bottom()) {
          continue; // Already handled.
        }

        // Read semantics: we propagate the elements to the children.
        auto other_subtree_copy = other_subtree;
        other_subtree_copy.elements_.join_with(other.elements_);

        // Compare with left_tree[*]
        if (!subtree_star.leq(other_subtree_copy)) {
          return false;
        }
      }
    }

    return true;
  }

  bool equals(const AbstractTreeDomain& other) const {
    if (!elements_.equals(other.elements_)) {
      return false;
    }

    if (children_.reference_equals(other.children_)) {
      return true;
    }

    for (const auto& [path_element, subtree] : children_) {
      auto subtree_copy = subtree;
      auto other_subtree = other.children_.at(path_element);

      // Read semantics: we propagate the elements to the children.
      subtree_copy.elements_.join_with(elements_);
      other_subtree.elements_.join_with(other.elements_);

      if (!subtree_copy.equals(other_subtree)) {
        return false;
      }
    }

    for (const auto& [path_element, other_subtree] : other.children_) {
      auto subtree = children_.at(path_element);

      if (!subtree.is_bottom()) {
        continue; // Already handled.
      }

      // Read semantics: we propagate the elements to the children.
      auto other_subtree_copy = other_subtree;
      other_subtree_copy.elements_.join_with(other.elements_);
      subtree = AbstractTreeDomain(elements_);

      if (!subtree.equals(other_subtree_copy)) {
        return false;
      }
    }

    return true;
  }

  void join_with(const AbstractTreeDomain& other) {
    mt_if_expensive_assert(auto previous = *this);

    if (other.is_bottom()) {
      return;
    } else if (is_bottom()) {
      *this = other;
    } else {
      join_with_internal(other, Elements::bottom());
    }

    mt_expensive_assert(previous.leq(*this) && other.leq(*this));
  }

 private:
  /**
   * Merging is tricky because of the special meaning of [*] and [f].
   * We have to consider the three sets of indices:
   * L : indices [f] only in left_tree
   * R : indices [f] only in right_tree
   * C : indices [f] common in left_tree and right_tree.
   *
   * The merge result joined is then:
   * - joined.element = pointwise merge of left_tree.element and
   *     right_tree.element (if element is a field)
   * - joined[*] = left_tree[*] merge right_tree[*]
   * - joined[c] = left_tree[c] merge right_tree[c] if c in C
   * - joined[l] = left_tree[l] merge right_tree[*] if l in L
   * - joined[r] = right_tree[r] merge left_tree[*] if r in R
   */
  void join_with_internal(
      const AbstractTreeDomain& other,
      const Elements& accumulator) {
    // The read semantics implies that an element on a node is implicitly
    // propagated to all its children. The `accumulator` contains all elements
    // of the ancestors/parents. If the elements on a child are included in
    // the accumulator, we can remove them.
    elements_.join_with(other.elements_);
    elements_.difference_with(accumulator);

    if (children_.reference_equals(other.children_)) {
      return;
    }

    const auto new_accumulator_tree = AbstractTreeDomain{
        Configuration::transform_on_sink(accumulator.join(elements_))};
    Map new_children;
    const auto& subtree_star = children_.at(PathElement::any_index());
    const auto& other_subtree_star =
        other.children_.at(PathElement::any_index());

    // Cases:
    // - joined.element = pointwise merge of left_tree.element and
    //   right_tree.element (if element is a field in left_tree)
    // - joined[*] = left_tree[*] merge right_tree[*] (if left_tree[*] exists)
    // - joined[c] = left_tree[c] merge right_tree[c] for c in C
    // - joined[l] = left_tree[l] merge right_tree[*] for l in L
    for (const auto& [path_element, subtree] : children_) {
      // Default to right_tree[*] for set of indices L
      const auto& other_subtree = get_element_or_star(
          other.children_, path_element, other_subtree_star);

      update_children_at_path_element(
          new_children,
          path_element,
          new_accumulator_tree,
          subtree,
          other_subtree);
    }

    for (const auto& [path_element, other_subtree] : other.children_) {
      const auto& subtree = children_.at(path_element);

      if (!subtree.is_bottom()) {
        // Cases already handled:
        // - joined.element = pointwise merge of left_tree.element and
        //   right_tree.element (if element is a field in left_tree)
        // - joined[c] = left_tree[c] merge right_tree[c] for c in C
        continue;
      }

      if (path_element.is_index()) {
        // Case: joined[r] = right_tree[r] merge left_tree[*] for r in R
        update_children_at_path_element(
            new_children,
            path_element,
            new_accumulator_tree,
            subtree_star,
            other_subtree);
      } else {
        // Cases:
        // - joined.element = pointwise merge of right_tree.element and
        //   left_tree.element (if element is a field in right_tree only)
        // - joined[*] = right_tree[*] merge left_tree[*] (if left_tree[*]
        //   did not exists)
        update_children_at_path_element(
            new_children,
            path_element,
            new_accumulator_tree,
            other_subtree,
            subtree);
      }
    }

    children_ = new_children;
  }

  void update_children_at_path_element(
      Map& children,
      PathElement path_element,
      const AbstractTreeDomain& accumulator_tree,
      const AbstractTreeDomain& left_subtree,
      const AbstractTreeDomain& right_subtree) {
    if (!right_subtree.is_bottom()) {
      auto left_subtree_copy = left_subtree;
      left_subtree_copy.join_with_internal(
          right_subtree, accumulator_tree.elements_);

      if (!left_subtree_copy.is_bottom()) {
        children.insert_or_assign(path_element, std::move(left_subtree_copy));
      }
    } else {
      if (!left_subtree.leq(accumulator_tree)) {
        auto left_subtree_copy = left_subtree;
        left_subtree_copy.join_with_internal(
            right_subtree, accumulator_tree.elements_);

        if (!left_subtree_copy.is_bottom()) {
          children.insert_or_assign(path_element, std::move(left_subtree_copy));
        }
      }
    }
  }

 public:
  void widen_with(const AbstractTreeDomain& other) {
    mt_if_expensive_assert(auto previous = *this);

    if (other.is_bottom()) {
      return;
    } else if (is_bottom()) {
      *this = other;
    } else {
      widen_with_internal(
          other,
          Elements::bottom(),
          Configuration::max_tree_height_after_widening());
    }

    mt_expensive_assert(previous.leq(*this) && other.leq(*this));
  }

 private:
  void widen_with_internal(
      const AbstractTreeDomain& other,
      const Elements& accumulator,
      std::size_t max_height) {
    if (max_height == 0) {
      collapse_inplace(Configuration::transform_on_widening_collapse);
      elements_.join_with(
          other.collapse(Configuration::transform_on_widening_collapse));
      elements_.difference_with(accumulator);
      return;
    }

    // The read semantics implies that an element on a node is implicitly
    // propagated to all its children. The `accumulator` contains all elements
    // of the ancestors/parents. If the elements on a child are included in
    // the accumulator, we can remove them.
    elements_.join_with(other.elements_);
    elements_.difference_with(accumulator);

    if (children_.reference_equals(other.children_)) {
      collapse_deeper_than(
          max_height, Configuration::transform_on_widening_collapse);
      return;
    }

    const auto new_accumulator_tree = AbstractTreeDomain{
        Configuration::transform_on_sink(accumulator.join(elements_))};
    Map new_children;

    for (const auto& [path_element, subtree] : children_) {
      const auto& other_subtree = other.children_.at(path_element);

      if (!other_subtree.is_bottom()) {
        auto subtree_copy = subtree;
        subtree_copy.widen_with_internal(
            other_subtree, new_accumulator_tree.elements_, max_height - 1);

        if (!subtree_copy.is_bottom()) {
          new_children.insert_or_assign(path_element, std::move(subtree_copy));
        }
      } else {
        if (!subtree.leq(new_accumulator_tree)) {
          auto subtree_copy = subtree;
          subtree_copy.collapse_deeper_than(
              max_height - 1, Configuration::transform_on_widening_collapse);
          new_children.insert_or_assign(path_element, subtree_copy);
        }
      }
    }

    for (const auto& [path_element, other_subtree] : other.children_) {
      const auto& subtree = children_.at(path_element);

      if (!subtree.is_bottom()) {
        continue; // Already handled.
      }

      if (!other_subtree.leq(new_accumulator_tree)) {
        auto other_subtree_copy = other_subtree;
        other_subtree_copy.collapse_deeper_than(
            max_height - 1, Configuration::transform_on_widening_collapse);
        new_children.insert_or_assign(path_element, other_subtree_copy);
      }
    }

    children_ = new_children;
  }

 public:
  void meet_with(const AbstractTreeDomain& /*other*/) {
    mt_unreachable(); // Not implemented.
  }

  void narrow_with(const AbstractTreeDomain& other) {
    meet_with(other);
  }

  /* Return all elements in the tree. Elements are collapsed unchanged. */
  Elements collapse() const {
    Elements elements = elements_;
    for (const auto& [path_element, subtree] : children_) {
      subtree.merge_into(elements, Configuration::transform_on_hoist);
    }
    return elements;
  }

  /**
   * Return all elements in the tree.
   *
   * `transform` is a function that is called when collapsing `Element`s into
   * the root. This is mainly used to attach broadening features to collapsed
   * taint.
   */
  template <typename Transform> // Elements(Elements)
  Elements collapse(Transform&& transform) const {
    Elements elements = elements_;
    for (const auto& [path_element, subtree] : children_) {
      subtree.merge_into(elements, [&transform](Elements value) {
        return transform(Configuration::transform_on_hoist(std::move(value)));
      });
    }
    return elements;
  }

  /* Collapse the tree into a singleton, in place. */
  void collapse_inplace() {
    for (const auto& [path_element, subtree] : children_) {
      subtree.merge_into(elements_, Configuration::transform_on_hoist);
    }
    children_.clear();
  }

  /*
   * Collapse the tree into a singleton, in place.
   *
   * `transform` is a function that is called when collapsing `Element`s into
   * the root. This is mainly used to attach broadening features to collapsed
   * taint.
   */
  template <typename Transform> // Elements(Elements)
  void collapse_inplace(Transform&& transform) {
    for (const auto& [path_element, subtree] : children_) {
      subtree.merge_into(elements_, [&transform](Elements value) {
        return transform(Configuration::transform_on_hoist(std::move(value)));
      });
    }
    children_.clear();
  }

 private:
  /* Join all elements in the tree into the given set of elements.
   *
   * The given `transform` function is applied on all elements (including the
   * root).
   *
   * Note: This does NOT call `transform_on_hoist`.
   */
  template <typename Transform> // Elements(Elements)
  void merge_into(Elements& elements, Transform&& transform) const {
    elements.join_with(transform(elements_));
    for (const auto& [_, subtree] : children_) {
      subtree.merge_into(elements, transform);
    }
  }

 public:
  /* Collapse the tree to the given maximum height. */
  void collapse_deeper_than(std::size_t height) {
    if (height == 0) {
      collapse_inplace();
    } else {
      children_.transform([height](AbstractTreeDomain subtree) {
        subtree.collapse_deeper_than(height - 1);
        return subtree;
      });
    }
  }

  /* Collapse the tree to the given maximum height. */
  template <typename Transform> // Elements(Elements)
  void collapse_deeper_than(std::size_t height, Transform&& transform) {
    static_assert(std::is_same_v<
                  decltype(transform(std::declval<Elements&&>())),
                  Elements>);

    if (height == 0) {
      collapse_inplace(std::forward<Transform>(transform));
    } else {
      children_.transform(
          [height, transform = std::forward<Transform>(transform)](
              AbstractTreeDomain subtree) {
            subtree.collapse_deeper_than(height - 1, transform);
            return subtree;
          });
    }
  }

  /* Remove the given elements from the tree. */
  void prune(Elements accumulator) {
    elements_.difference_with(accumulator);
    accumulator.join_with(elements_);
    prune_children(Configuration::transform_on_sink(std::move(accumulator)));
  }

  /* Remove the given elements from the subtrees. */
  void prune_children(const Elements& accumulator) {
    children_.transform([&accumulator](AbstractTreeDomain subtree) {
      subtree.prune(accumulator);
      return subtree;
    });
  }

  /**
   * When a path is invalid, collapse its taint into its parent's.
   *
   * A path is invalid if `is_valid().first` is `false`. If valid, the
   * Accumulator contains information about visited paths so far.
   */
  template <typename Accumulator>
  void collapse_invalid_paths(
      const std::function<
          std::pair<bool, Accumulator>(const Accumulator&, PathElement)>&
          is_valid,
      const Accumulator& accumulator,
      const std::function<Elements(Elements)>& transform_on_collapse) {
    Map new_children;
    for (const auto& [path_element, subtree] : children_) {
      const auto& [valid, accumulator_for_subtree] =
          is_valid(accumulator, path_element);
      if (!valid) {
        // Invalid path, collapse subtree into current tree.
        subtree.merge_into(elements_, [&transform_on_collapse](Elements value) {
          return transform_on_collapse(
              Configuration::transform_on_hoist(std::move(value)));
        });
      } else {
        auto subtree_copy = subtree;
        subtree_copy.collapse_invalid_paths(
            is_valid, accumulator_for_subtree, transform_on_collapse);
        new_children.insert_or_assign(path_element, std::move(subtree_copy));
      }
    }
    children_ = new_children;
  }

  /* Collapse children that have more than `max_leaves` leaves. */
  void limit_leaves(std::size_t max_leaves) {
    auto depth = depth_exceeding_max_leaves(max_leaves);
    if (!depth) {
      return;
    }
    collapse_deeper_than(*depth);
  }

  /*
   * Collapse children that have more than `max_leaves` leaves.
   *
   * `transform` is a function applied to the `Element`s that are collapsed.
   * Mainly used to add broadening features to collapsed taint
   */
  template <typename Transform> // Elements(Elements)
  void limit_leaves(std::size_t max_leaves, Transform&& transform) {
    auto depth = depth_exceeding_max_leaves(max_leaves);
    if (!depth) {
      return;
    }
    collapse_deeper_than(*depth, std::forward<Transform>(transform));
  }

  /* Return the depth at which the tree exceeds the given number of leaves. */
  std::optional<std::size_t> depth_exceeding_max_leaves(
      std::size_t max_leaves) const {
    // Set of trees at the current depth.
    std::vector<const AbstractTreeDomain*> trees = {this};
    std::size_t depth = 0;

    // Breadth-first search.
    while (!trees.empty()) {
      std::vector<const AbstractTreeDomain*> new_trees;

      for (const auto* tree : trees) {
        for (const auto& [path_element, subtree] : tree->children_) {
          if (subtree.children_.empty()) {
            if (max_leaves > 0) {
              max_leaves--;
            } else {
              return depth;
            }
          } else {
            new_trees.push_back(&subtree);
          }
        }
      }

      if (new_trees.size() > max_leaves) {
        return depth;
      }

      depth++;
      trees = std::move(new_trees);
    }

    return std::nullopt;
  }

  /* Write the given elements at the given path. */
  void write(const Path& path, Elements elements, UpdateKind kind) {
    write_internal(
        path.begin(),
        path.end(),
        std::move(elements),
        Elements::bottom(),
        kind);
  }

 private:
  void write_internal(
      Path::ConstIterator begin,
      Path::ConstIterator end,
      Elements elements,
      Elements accumulator,
      UpdateKind kind) {
    if (begin == end) {
      switch (kind) {
        case UpdateKind::Strong: {
          elements_ = std::move(elements);
          children_.clear();
          break;
        }
        case UpdateKind::Weak: {
          elements_.join_with(elements);
          accumulator.join_with(elements_);
          prune_children(
              Configuration::transform_on_sink(std::move(accumulator)));
          break;
        }
      }
      return;
    }

    accumulator.join_with(elements_);
    elements.difference_with(accumulator);

    if (elements.is_bottom() && kind == UpdateKind::Weak) {
      return;
    }

    auto path_head = *begin;
    ++begin;

    if (path_head.is_index() && kind == UpdateKind::Weak) {
      // Merge in existing [*] for weak write on new index:
      // If we are weak assigning to a new index and the tree already consists
      // of a path element [*], we need to merge [*] with the index as the
      // existing [*] also covered this index.
      if (children_.at(path_head).is_bottom()) {
        auto new_subtree = children_.at(PathElement::any_index());
        if (!new_subtree.is_bottom()) {
          children_.insert_or_assign(path_head, new_subtree);
        }
      }
    }

    accumulator = Configuration::transform_on_sink(std::move(accumulator));

    if (path_head.is_any_index()) {
      // Write on any_index [*] == write on every index:
      // [*] has a different meaning for the write() api than the [*] node in
      // the tree.
      //   - node [*] in the tree represents any remaining index apart from the
      //   index already present in the tree.
      //   - write([*]) implies write to an unknown/unresolved index which could
      //   be some index we know about or any other index. In this sense, it
      //   represents _every_ index.
      // Hence, we consider write() to [*] as weak write() to every index.
      kind = UpdateKind::Weak;
      Map new_children;

      for (const auto& [path_element, subtree] : children_) {
        auto new_subtree = subtree;

        if (path_element.is_index()) {
          new_subtree.write_internal(begin, end, elements, accumulator, kind);
        }

        if (!new_subtree.is_bottom()) {
          new_children.insert_or_assign(path_element, std::move(new_subtree));
        }
      }

      children_ = new_children;
    }

    children_.update(
        [begin, end, &elements, &accumulator, kind](const auto& subtree) {
          auto new_subtree = subtree;
          new_subtree.write_internal(
              begin, end, std::move(elements), std::move(accumulator), kind);
          return new_subtree;
        },
        path_head);
  }

 public:
  /* Write the given tree at the given path. */
  void write(const Path& path, AbstractTreeDomain tree, UpdateKind kind) {
    write_internal(
        path.begin(), path.end(), std::move(tree), Elements::bottom(), kind);
  }

 private:
  void write_internal(
      Path::ConstIterator begin,
      Path::ConstIterator end,
      AbstractTreeDomain tree,
      Elements accumulator,
      UpdateKind kind) {
    if (begin == end) {
      switch (kind) {
        case UpdateKind::Strong: {
          *this = std::move(tree);
          prune(std::move(accumulator));
          break;
        }
        case UpdateKind::Weak: {
          join_with_internal(tree, accumulator);
          break;
        }
      }
      return;
    }

    accumulator.join_with(elements_);

    auto path_head = *begin;
    ++begin;

    // Merge in existing [*] for weak write on new index.
    if (path_head.is_index() && kind == UpdateKind::Weak) {
      if (children_.at(path_head).is_bottom()) {
        tree.join_with(children_.at(PathElement::any_index()));
        tree.elements_.difference_with(accumulator);
      }
    }

    accumulator = Configuration::transform_on_sink(std::move(accumulator));

    if (path_head.is_any_index()) {
      // Write on any_index [*] == write on every index.
      kind = UpdateKind::Weak;
      Map new_children;

      for (const auto& [path_element, subtree] : children_) {
        auto new_subtree = subtree;

        if (path_element.is_index()) {
          new_subtree.write_internal(begin, end, tree, accumulator, kind);
        }

        if (!new_subtree.is_bottom()) {
          new_children.insert_or_assign(path_element, std::move(new_subtree));
        }
      }

      children_ = new_children;
    }

    children_.update(
        [begin, end, &tree, &accumulator, kind](const auto& subtree) {
          auto new_subtree = subtree;
          new_subtree.write_internal(
              begin, end, std::move(tree), std::move(accumulator), kind);
          return new_subtree;
        },
        path_head);
  }

 public:
  /**
   * Return the subtree at the given path.
   *
   * `propagate` is a function that is called when propagating elements down to
   * a child. This is mainly used to attach the correct access path to
   * backward taint to infer propagations.
   */
  template <typename Propagate> // Elements(Elements, Path::Element)
  AbstractTreeDomain read(const Path& path, Propagate&& propagate) const {
    static_assert(
        std::is_same_v<
            decltype(propagate(
                std::declval<Elements&&>(), std::declval<Path::Element>())),
            Elements>);

    return read_internal(
        path.begin(), path.end(), std::forward<Propagate>(propagate));
  }

  /**
   * Return the subtree at the given path.
   *
   * Elements are propagated down to children unchanged.
   */
  AbstractTreeDomain read(const Path& path) const {
    return read_internal(
        path.begin(),
        path.end(),
        [](Elements elements, Path::Element /*path_element*/) -> Elements {
          return elements;
        });
  }

 private:
  template <typename Propagate>
  AbstractTreeDomain read_internal(
      Path::ConstIterator begin,
      Path::ConstIterator end,
      Propagate&& propagate) const {
    if (begin == end) {
      return *this;
    }
    auto path_head = *begin;
    ++begin;

    auto subtree = children_.at(path_head);
    if (path_head.is_index() && subtree.is_bottom()) {
      // Read from any_index [*] if the index is not in the tree.
      subtree = children_.at(PathElement::any_index());
    } else if (path_head.is_any_index()) {
      // Read from [*] == read from every index
      for (const auto& [path_element, index_subtree] : children_) {
        if (!path_element.is_index()) {
          continue;
        }

        subtree.join_with(index_subtree);
      }
    }

    if (subtree.is_bottom()) {
      auto result =
          Configuration::transform_on_sink(propagate(elements_, path_head));
      for (; begin != end; ++begin) {
        result = Configuration::transform_on_sink(propagate(result, *begin));
      }
      return AbstractTreeDomain(result);
    }

    subtree.elements_.join_with(
        Configuration::transform_on_sink(propagate(elements_, path_head)));
    return subtree.read_internal(
        begin, end, std::forward<Propagate>(propagate));
  }

 public:
  /**
   * Return the subtree at the given path.
   *
   * Elements are NOT propagated down to children.
   */
  AbstractTreeDomain raw_read(const Path& path) const {
    return raw_read_internal(path.begin(), path.end());
  }

 private:
  AbstractTreeDomain raw_read_internal(
      Path::ConstIterator begin,
      Path::ConstIterator end) const {
    if (is_bottom() || begin == end) {
      return *this;
    }

    auto subtree = children_.at(*begin);

    return subtree.raw_read_internal(std::next(begin), end);
  }

 public:
  /**
   * Return the subtree at the given path and the remaining path elements if the
   * full path did not exist in the tree.
   *
   * Elements are NOT propagated down to children.
   */
  std::pair<Path, AbstractTreeDomain> raw_read_max_path(
      const Path& path) const {
    return raw_read_max_path_internal(path.begin(), path.end());
  }

 private:
  std::pair<Path, AbstractTreeDomain> raw_read_max_path_internal(
      Path::ConstIterator begin,
      Path::ConstIterator end) const {
    if (is_bottom() || begin == end) {
      return std::make_pair(Path{begin, end}, *this);
    }

    auto subtree = children_.at(*begin);
    if (subtree.is_bottom()) {
      return std::make_pair(Path{begin, end}, *this);
    }

    return subtree.raw_read_max_path_internal(std::next(begin), end);
  }

 public:
  /**
   * Transforms the tree so it only contains branches present in `mold`.
   *
   * When a branch is not present in `mold`, it is collapsed in its parent.
   * `transform` is a function called when collapsing. This is mainly used to
   * attach broadening features to collapsed taint.
   */
  template <typename Transform> // Elements(Elements)
  void shape_with(const AbstractTreeDomain& mold, Transform&& transform) {
    static_assert(std::is_same_v<
                  decltype(transform(std::declval<Elements&&>())),
                  Elements>);

    shape_with_internal(
        mold, std::forward<Transform>(transform), Elements::bottom());
  }

 private:
  template <typename Transform> // Elements(Elements)
  void shape_with_internal(
      const AbstractTreeDomain& mold,
      Transform&& transform,
      const Elements& accumulator) {
    const auto& mold_any_index_subtree =
        mold.children_.at(PathElement::any_index());
    bool mold_has_any_index = !mold_any_index_subtree.is_bottom();

    // First pass: collapse branches, so we can build a new accumulator.
    Map new_children;
    for (const auto& [path_element, subtree] : children_) {
      const auto& mold_subtree = mold.children_.at(path_element);

      if (!mold_subtree.is_bottom()) {
        new_children.insert_or_assign(path_element, subtree);
      } else if (
          mold_has_any_index &&
          path_element.kind() == PathElement::Kind::Index) {
        // Keep `Index` branches when the mold has an `AnyIndex` branch.
        new_children.insert_or_assign(path_element, subtree);
      } else {
        subtree.merge_into(elements_, [&transform](Elements value) {
          return transform(Configuration::transform_on_hoist(std::move(value)));
        });
      }
    }

    elements_.difference_with(accumulator);
    auto new_accumulator =
        Configuration::transform_on_sink(accumulator.join(elements_));

    // Second pass: apply shape_with on children.
    children_.clear();
    for (const auto& [path_element, subtree] : new_children) {
      const auto& mold_subtree = mold.children_.at(path_element);

      auto new_subtree = subtree;
      if (!mold_subtree.is_bottom()) {
        new_subtree.shape_with_internal(
            mold_subtree, transform, new_accumulator);
      } else if (
          mold_has_any_index &&
          path_element.kind() == PathElement::Kind::Index) {
        // The tree may contain extra `Index` branches when the mold has an
        // `AnyIndex` branch.
        new_subtree.shape_with_internal(
            mold_any_index_subtree, transform, new_accumulator);
      } else {
        mt_unreachable_log("invariant broken in shape_with");
      }

      children_.update(
          [&new_subtree](const auto& subtree) {
            return subtree.join(new_subtree);
          },
          path_element);
    }
  }

 public:
  /**
   * Iterate on all non-empty elements in the tree.
   *
   * When visiting the tree, elements do not include their ancestors.
   */
  template <typename Visitor> // void(const Path&, const Elements&)
  void visit(Visitor&& visitor) const {
    static_assert(
        std::is_same_v<
            decltype(visitor(
                std::declval<const Path>(), std::declval<const Elements>())),
            void>);

    Path path;
    visit_internal(path, std::forward<Visitor>(visitor));
  }

 private:
  template <typename Visitor> // void(const Path&, const Elements&)
  void visit_internal(Path& path, Visitor&& visitor) const {
    if (!elements_.is_bottom()) {
      visitor(path, elements_);
    }

    for (const auto& [path_element, subtree] : children_) {
      path.append(path_element);
      subtree.visit_internal(path, visitor);
      path.pop_back();
    }
  }

 public:
  /**
   * Iterate on all non-empty elements in the tree in the post-order.
   *
   * When visiting the tree, elements do not include their ancestors.
   */
  template <typename Visitor> // void(const Path&, const Elements&)
  void visit_postorder(Visitor&& visitor) const {
    static_assert(
        std::is_same_v<
            decltype(visitor(
                std::declval<const Path>(), std::declval<const Elements>())),
            void>);

    Path path;
    visit_postorder_internal(path, std::forward<Visitor>(visitor));
  }

 private:
  template <typename Visitor> // void(const Path&, const Elements&)
  void visit_postorder_internal(Path& path, Visitor&& visitor) const {
    for (const auto& [path_element, subtree] : children_) {
      path.append(path_element);
      subtree.visit_internal(path, visitor);
      path.pop_back();
    }

    if (!elements_.is_bottom()) {
      visitor(path, elements_);
    }
  }

 public:
  /**
   * Return the list of all pairs (path, elements) in the tree.
   *
   * Elements are returned by reference.
   * Elements do not contain their ancestors.
   */
  std::vector<std::pair<Path, const Elements&>> elements() const {
    std::vector<std::pair<Path, const Elements&>> results;
    visit([&results](const Path& path, const Elements& elements) {
      results.push_back({path, elements});
    });
    return results;
  }

  /* Apply the given function on all elements. */
  template <typename Function> // Elements(Elements)
  void transform(Function&& f) {
    static_assert(
        std::is_same_v<decltype(f(std::declval<Elements&&>())), Elements>);

    transform_internal(std::forward<Function>(f), Elements::bottom());
  }

 private:
  template <typename Function> // Elements(Elements)
  void transform_internal(Function&& f, Elements accumulator) {
    if (!elements_.is_bottom()) {
      elements_ = f(std::move(elements_));
      elements_.difference_with(accumulator);
      accumulator.join_with(elements_);
    }

    accumulator = Configuration::transform_on_sink(std::move(accumulator));

    children_.transform(
        [f = std::forward<Function>(f), &accumulator](AbstractTreeDomain tree) {
          tree.transform_internal(f, accumulator);
          return tree;
        });
  }

 public:
  friend std::ostream& operator<<(
      std::ostream& out,
      const AbstractTreeDomain& tree) {
    return tree.write(out, "");
  }

 private:
  std::ostream& write(std::ostream& out, const std::string& indent) const {
    out << "{";
    if (is_bottom()) {
      return out << "}";
    } else if (!elements_.is_bottom() && children_.empty()) {
      return out << elements_ << "}";
    } else {
      auto new_indent = indent + "    ";
      if (!elements_.is_bottom()) {
        out << "\n" << new_indent << elements_;
      }
      for (const auto& [path_element, subtree] : children_) {
        out << "\n" << new_indent << "`" << show(path_element) << "` -> ";
        subtree.write(out, new_indent);
      }
      return out << "\n" << indent << "}";
    }
  }

 private:
  // The abstract elements at this node.
  // In theory, this includes all the elements from the ancestors.
  // In practice, we only store new elements.
  Elements elements_;

  // The edges to the child nodes.
  Map children_;
};

} // namespace marianatrench

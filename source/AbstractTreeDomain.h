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

#include <AbstractDomain.h>
#include <PatriciaTreeMap.h>
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
  auto& subtree = children.at(path_element.encode());

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
 * This implements an iterator over a map which transforms the key of type
 * `PathElement::ElementEncoding` to a concrete `PathElement`.
 */
template <typename Map>
class PathElementMapIterator final {
  static_assert(
      std::is_same_v<typename Map::key_type, PathElement::ElementEncoding>);

  template <typename Value>
  struct ExposeBinding {
    const std::pair<PathElement, Value>& operator()(
        const std::pair<typename PathElement::ElementEncoding, Value>& pair)
        const {
      // This is safe as `PathElement` stores `PathElement::ElementEncoding`
      // internally.
      return *reinterpret_cast<const std::pair<PathElement, Value>*>(&pair);
    }
  };

 public:
  // C++ container concept member types
  using key_type = PathElement;
  using mapped_type = typename Map::mapped_type;
  using value_type = std::pair<key_type, mapped_type>;
  using iterator = boost::
      transform_iterator<ExposeBinding<mapped_type>, typename Map::iterator>;
  using const_iterator = iterator;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using const_reference = const value_type&;
  using const_pointer = const value_type*;

 private:
  // Safety checks of `boost::transform_iterator`.
  static_assert(std::is_same_v<typename iterator::value_type, value_type>);
  static_assert(
      std::is_same_v<typename iterator::difference_type, difference_type>);
  static_assert(std::is_same_v<typename iterator::reference, const_reference>);
  static_assert(std::is_same_v<typename iterator::pointer, const_pointer>);

 public:
  explicit PathElementMapIterator(Map& map) : map_(map) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(PathElementMapIterator)

  iterator begin() const {
    return boost::make_transform_iterator(
        map_.begin(), ExposeBinding<mapped_type>());
  }

  iterator end() const {
    return boost::make_transform_iterator(
        map_.end(), ExposeBinding<mapped_type>());
  }

  size_t size() const {
    return map_.size();
  }

  bool empty() const {
    return map_.empty();
  }

 private:
  Map& map_;
};

/**
 * An abstract tree domain.
 *
 * This is mainly used with a source set or a sink set as `Elements`, to store
 * the taint on each access paths.
 *
 * Elements on nodes are implicitly propagated to their children.
 */
template <typename Elements>
class AbstractTreeDomain final
    : public sparta::AbstractDomain<AbstractTreeDomain<Elements>> {
 public:
  using PathElement = typename Path::Element;

 private:
  struct ValueInterface {
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
  };

 public:
  using Map = sparta::PatriciaTreeMap<
      PathElement::ElementEncoding,
      AbstractTreeDomain,
      ValueInterface>;

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

  bool is_bottom() const override {
    return elements_.is_bottom() && children_.empty();
  }

  bool is_top() const override {
    return false;
  }

  void set_to_bottom() override {
    elements_.set_to_bottom();
    children_.clear();
  }

  void set_to_top() override {
    mt_unreachable(); // Not implemented.
  }

  const Elements& root() const {
    return elements_;
  }

  const PathElementMapIterator<const Map> successors() const {
    return PathElementMapIterator(children_);
  }

  const AbstractTreeDomain& successor(PathElement path_element) const {
    return children_.at(path_element.encode());
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
  bool leq(const AbstractTreeDomain& other) const override {
    // Case: left_tree.elements <= right_tree.elements
    if (!elements_.leq(other.elements_)) {
      return false;
    }

    if (children_.reference_equals(other.children_)) {
      return true;
    }

    const auto& other_subtree_star =
        other.children_.at(PathElement::any_index().encode());

    // Cases:
    //  left_tree[c] <= right_tree[c] for all c in C /\
    //  left_tree[*] <= right_tree[*] if left_tree[*] present /\
    //  left_tree[l] <= right_tree[*] for all l in L.
    for (const auto& [path_element, subtree] :
         PathElementMapIterator(children_)) {
      // Default to right_tree[*] for set of indices L
      auto other_subtree_copy = get_element_or_star(
          other.children_, path_element, other_subtree_star);

      // Read semantics: we propagate the elements to the children.
      other_subtree_copy.elements_.join_with(other.elements_);

      if (!subtree.leq(other_subtree_copy)) {
        return false;
      }
    }

    const auto& subtree_star = children_.at(PathElement::any_index().encode());

    // Cases:
    //  left_tree[*] <= right_tree[r] for all r in R /\
    //  left_tree[*] <= right_tree[*] if right_tree[*] present.
    for (const auto& [path_element, other_subtree] :
         PathElementMapIterator(other.children_)) {
      if (path_element.is_field()) {
        continue;
      }

      const auto& subtree = children_.at(path_element.encode());

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

    return true;
  }

  bool equals(const AbstractTreeDomain& other) const override {
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

  void join_with(const AbstractTreeDomain& other) override {
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

    const auto new_accumulator_tree =
        AbstractTreeDomain{accumulator.join(elements_)};
    Map new_children;
    const auto& subtree_star = children_.at(PathElement::any_index().encode());
    const auto& other_subtree_star =
        other.children_.at(PathElement::any_index().encode());

    // Cases:
    // - joined.element = pointwise merge of left_tree.element and
    //   right_tree.element (if element is a field in left_tree)
    // - joined[*] = left_tree[*] merge right_tree[*] (if left_tree[*] exists)
    // - joined[c] = left_tree[c] merge right_tree[c] for c in C
    // - joined[l] = left_tree[l] merge right_tree[*] for l in L
    for (const auto& [path_element, subtree] :
         PathElementMapIterator(children_)) {
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

    for (const auto& [path_element, other_subtree] :
         PathElementMapIterator(other.children_)) {
      const auto& subtree = children_.at(path_element.encode());

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
        children.insert_or_assign(
            path_element.encode(), std::move(left_subtree_copy));
      }
    } else {
      if (!left_subtree.leq(accumulator_tree)) {
        auto left_subtree_copy = left_subtree;
        left_subtree_copy.join_with_internal(
            right_subtree, accumulator_tree.elements_);

        if (!left_subtree_copy.is_bottom()) {
          children.insert_or_assign(
              path_element.encode(), std::move(left_subtree_copy));
        }
      }
    }
  }

 public:
  void widen_with(const AbstractTreeDomain& other) override {
    mt_if_expensive_assert(auto previous = *this);

    if (other.is_bottom()) {
      return;
    } else if (is_bottom()) {
      *this = other;
    } else {
      widen_with_internal(
          other,
          Elements::bottom(),
          /* max_height */ Heuristics::kAbstractTreeWideningHeight);
    }

    mt_expensive_assert(previous.leq(*this) && other.leq(*this));
  }

 private:
  void widen_with_internal(
      const AbstractTreeDomain& other,
      const Elements& accumulator,
      std::size_t max_height) {
    if (max_height == 0) {
      collapse_inplace();
      other.collapse_into(elements_);
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
      collapse_deeper_than(max_height);
      return;
    }

    const auto new_accumulator_tree =
        AbstractTreeDomain{accumulator.join(elements_)};
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
          subtree_copy.collapse_deeper_than(max_height - 1);
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
        other_subtree_copy.collapse_deeper_than(max_height - 1);
        new_children.insert_or_assign(path_element, other_subtree_copy);
      }
    }

    children_ = new_children;
  }

 public:
  void meet_with(const AbstractTreeDomain& /*other*/) override {
    mt_unreachable(); // Not implemented.
  }

  void narrow_with(const AbstractTreeDomain& other) override {
    meet_with(other);
  }

  /* Return all elements in the tree. Elements are collapsed unchanged. */
  Elements collapse() const {
    Elements elements = elements_;
    for (const auto& [path_element, subtree] : children_) {
      subtree.collapse_into(elements);
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
  Elements collapse(const std::function<void(Elements&)>& transform) const {
    Elements elements = elements_;
    for (const auto& [path_element, subtree] : children_) {
      subtree.collapse_into(elements, transform);
    }
    return elements;
  }

  /* Collapse the tree into a singleton, in place. */
  void collapse_inplace() {
    for (const auto& [path_element, subtree] : children_) {
      subtree.collapse_into(elements_);
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
  void collapse_inplace(const std::function<void(Elements&)>& transform) {
    for (const auto& [path_element, subtree] : children_) {
      subtree.collapse_into(elements_, transform);
    }
    children_.clear();
  }

  /* Collapse the tree into the given set of elements. */
  void collapse_into(Elements& elements) const {
    elements.join_with(elements_);
    for (const auto& [_, subtree] : children_) {
      subtree.collapse_into(elements);
    }
  }

  /* Collapse the tree into the given set of elements. */
  void collapse_into(
      Elements& elements,
      const std::function<void(Elements&)>& transform) const {
    Elements elements_copy = elements_;
    transform(elements_copy);
    elements.join_with(elements_copy);
    for (const auto& [_, subtree] : children_) {
      subtree.collapse_into(elements, transform);
    }
  }

  /* Collapse the tree to the given maximum height. */
  void collapse_deeper_than(std::size_t height) {
    if (height == 0) {
      collapse_inplace();
    } else {
      children_.map([=](const AbstractTreeDomain& subtree) {
        auto copy = subtree;
        copy.collapse_deeper_than(height - 1);
        return copy;
      });
    }
  }

  /* Collapse the tree to the given maximum height. */
  void collapse_deeper_than(
      std::size_t height,
      const std::function<void(Elements&)>& transform) {
    if (height == 0) {
      collapse_inplace(transform);
    } else {
      children_.map([height, &transform](const AbstractTreeDomain& subtree) {
        auto copy = subtree;
        copy.collapse_deeper_than(height - 1, transform);
        return copy;
      });
    }
  }

  /* Remove the given elements from the tree. */
  void prune(Elements accumulator) {
    elements_.difference_with(accumulator);
    accumulator.join_with(elements_);
    prune_children(accumulator);
  }

  /* Remove the given elements from the subtrees. */
  void prune_children(const Elements& accumulator) {
    children_.map([&](const AbstractTreeDomain& subtree) {
      auto copy = subtree;
      copy.prune(accumulator);
      return copy;
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
      const Accumulator& accumulator) {
    Map new_children;
    for (const auto& [path_element, subtree] :
         PathElementMapIterator(children_)) {
      const auto& [valid, accumulator_for_subtree] =
          is_valid(accumulator, path_element);
      if (!valid) {
        // Invalid path, collapse subtree into current tree.
        elements_.join_with(subtree.collapse());
      } else {
        auto subtree_copy = subtree;
        subtree_copy.collapse_invalid_paths(is_valid, accumulator_for_subtree);
        new_children.insert_or_assign(
            path_element.encode(), std::move(subtree_copy));
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
  void limit_leaves(
      std::size_t max_leaves,
      const std::function<void(Elements&)>& transform) {
    auto depth = depth_exceeding_max_leaves(max_leaves);
    if (!depth) {
      return;
    }
    collapse_deeper_than(*depth, transform);
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
          prune_children(accumulator);
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
      if (children_.at(path_head.encode()).is_bottom()) {
        auto new_subtree = children_.at(PathElement::any_index().encode());
        if (!new_subtree.is_bottom()) {
          children_.insert_or_assign(path_head.encode(), new_subtree);
        }
      }
    } else if (path_head.is_any_index()) {
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

      for (const auto& [path_element, subtree] :
           PathElementMapIterator(children_)) {
        auto new_subtree = subtree;

        if (path_element.is_index()) {
          new_subtree.write_internal(begin, end, elements, accumulator, kind);
        }

        if (!new_subtree.is_bottom()) {
          new_children.insert_or_assign(
              path_element.encode(), std::move(new_subtree));
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
        path_head.encode());
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
      if (children_.at(path_head.encode()).is_bottom()) {
        tree.join_with(children_.at(PathElement::any_index().encode()));
        tree.elements_.difference_with(accumulator);
      }
    } else if (path_head.is_any_index()) {
      // Write on any_index [*] == write on every index.
      kind = UpdateKind::Weak;
      Map new_children;

      for (const auto& [path_element, subtree] :
           PathElementMapIterator(children_)) {
        auto new_subtree = subtree;

        if (path_element.is_index()) {
          new_subtree.write_internal(begin, end, tree, accumulator, kind);
        }

        if (!new_subtree.is_bottom()) {
          new_children.insert_or_assign(
              path_element.encode(), std::move(new_subtree));
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
        path_head.encode());
  }

 public:
  /**
   * Return the subtree at the given path.
   *
   * `propagate` is a function that is called when propagating elements down to
   * a child. This is mainly used to attach the correct access path to
   * artificial sources.
   */
  template <typename Propagate>
  AbstractTreeDomain read(const Path& path, const Propagate& propagate) const {
    return read_internal(path.begin(), path.end(), propagate);
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
      const Propagate& propagate) const {
    if (begin == end) {
      return *this;
    }
    auto path_head = *begin;
    ++begin;

    auto subtree = children_.at(path_head.encode());
    if (path_head.is_index() && subtree.is_bottom()) {
      // Read from any_index [*] if the index is not in the tree.
      subtree = children_.at(PathElement::any_index().encode());
    } else if (path_head.is_any_index()) {
      // Read from [*] == read from every index
      for (const auto& [path_element, index_subtree] :
           PathElementMapIterator(children_)) {
        if (!path_element.is_index()) {
          continue;
        }

        subtree.join_with(index_subtree);
      }
    }

    if (subtree.is_bottom()) {
      auto result = propagate(elements_, path_head);
      for (; begin != end; ++begin) {
        result = propagate(result, *begin);
      }
      return AbstractTreeDomain(result);
    }

    subtree.elements_.join_with(propagate(elements_, path_head));
    return subtree.read_internal(begin, end, propagate);
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

    auto subtree = children_.at(begin->encode());

    return subtree.raw_read_internal(std::next(begin), end);
  }

 public:
  /**
   * Iterate on all non-empty elements in the tree.
   *
   * When visiting the tree, elements do not include their ancestors.
   */
  void visit(std::function<void(const Path&, const Elements&)> visitor) const {
    Path path;
    visit_internal(path, visitor);
  }

 private:
  void visit_internal(
      Path& path,
      std::function<void(const Path&, const Elements&)>& visitor) const {
    if (!elements_.is_bottom()) {
      visitor(path, elements_);
    }

    for (const auto& [path_element, subtree] :
         PathElementMapIterator(children_)) {
      path.append(path_element);
      subtree.visit_internal(path, visitor);
      path.pop_back();
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
    visit([&](const Path& path, const Elements& elements) {
      results.push_back({path, elements});
    });
    return results;
  }

  /* Apply the given function on all elements. */
  void map(const std::function<void(Elements&)>& f) {
    map_internal(f, Elements::bottom());
  }

 private:
  void map_internal(
      const std::function<void(Elements&)>& f,
      Elements accumulator) {
    if (!elements_.is_bottom()) {
      f(elements_);
      elements_.difference_with(accumulator);
      accumulator.join_with(elements_);
    }

    children_.map([&](const AbstractTreeDomain& tree) {
      auto copy = tree;
      copy.map_internal(f, accumulator);
      return copy;
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
    out << "AbstractTree{";
    if (is_bottom()) {
      return out << "}";
    } else if (!elements_.is_bottom() && children_.empty()) {
      return out << elements_ << "}";
    } else {
      auto new_indent = indent + "    ";
      if (!elements_.is_bottom()) {
        out << "\n" << new_indent << elements_;
      }
      for (const auto& [path_element, subtree] :
           PathElementMapIterator(children_)) {
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

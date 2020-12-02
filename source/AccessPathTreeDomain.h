// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <functional>
#include <initializer_list>
#include <vector>

#include <AbstractDomain.h>
#include <Show.h>

#include <mariana-trench/AbstractTreeDomain.h>
#include <mariana-trench/Access.h>
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

  AccessPathTreeDomain(const AccessPathTreeDomain&) = default;
  AccessPathTreeDomain(AccessPathTreeDomain&&) = default;
  AccessPathTreeDomain& operator=(const AccessPathTreeDomain&) = default;
  AccessPathTreeDomain& operator=(AccessPathTreeDomain&&) = default;

  static AccessPathTreeDomain bottom() {
    return AccessPathTreeDomain(Map::bottom());
  }

  static AccessPathTreeDomain top() {
    return AccessPathTreeDomain(Map::top());
  }

  bool is_bottom() const override {
    return map_.is_bottom();
  }

  bool is_top() const override {
    return map_.is_top();
  }

  void set_to_bottom() override {
    map_.set_to_bottom();
  }

  void set_to_top() override {
    map_.set_to_top();
  }

  bool leq(const AccessPathTreeDomain& other) const override {
    return map_.leq(other.map_);
  }

  bool equals(const AccessPathTreeDomain& other) const override {
    return map_.equals(other.map_);
  }

  void join_with(const AccessPathTreeDomain& other) override {
    map_.join_with(other.map_);
  }

  void widen_with(const AccessPathTreeDomain& other) override {
    join_with(other);
  }

  void meet_with(const AccessPathTreeDomain& /*other*/) override {
    mt_unreachable(); // Not implemented.
  }

  void narrow_with(const AccessPathTreeDomain& other) override {
    meet_with(other);
  }

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

  /* Collapse children that have more than `max_leaves` leaves. */
  void limit_leaves(std::size_t max_leaves) {
    map_.map([=](const AbstractTreeDomainT& tree) {
      auto copy = tree;
      copy.limit_leaves(max_leaves);
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

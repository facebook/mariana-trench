/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <fmt/format.h>
#include <json/json.h>

#include <AbstractDomain.h>
#include <HashedSetAbstractDomain.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/Rule.h>
#include <mariana-trench/Taint.h>

namespace marianatrench {

using TextualOrderIndex = std::uint32_t;
constexpr std::string_view k_return_callee = "return";
constexpr std::string_view k_unresolved_callee = "unresolved";

namespace {
std::string get_handle(
    const std::string& callee,
    TextualOrderIndex sink_index,
    const Root& callee_port_root) {
  return fmt::format(
      "{}:{}:{}", callee, sink_index, callee_port_root.to_string());
}
} // namespace

class Issue final : public sparta::AbstractDomain<Issue> {
 public:
  using HandleSet = sparta::HashedSetAbstractDomain<std::string>;

  /* Create the bottom issue. */
  explicit Issue()
      : sources_(Taint::bottom()),
        sinks_(Taint::bottom()),
        rule_(nullptr),
        new_handles_(HandleSet({get_handle(
            std::string(k_return_callee),
            0,
            Root(Root::Kind::Leaf))})),
        position_(nullptr) {}

  explicit Issue(
      Taint sources,
      Taint sinks,
      const Rule* rule,
      const std::string& callee,
      TextualOrderIndex sink_index,
      const Root& callee_port_root,
      const Position* position)
      : sources_(std::move(sources)),
        sinks_(std::move(sinks)),
        rule_(rule),
        new_handles_(
            HandleSet({get_handle(callee, sink_index, callee_port_root)})),
        position_(position) {}

  explicit Issue(
      Taint sources,
      Taint sinks,
      const Rule* rule,
      HandleSet handles,
      const Position* position)
      : sources_(std::move(sources)),
        sinks_(std::move(sinks)),
        rule_(rule),
        new_handles_(handles),
        position_(position) {}

  Issue(const Issue&) = default;
  Issue(Issue&&) = default;
  Issue& operator=(const Issue&) = default;
  Issue& operator=(Issue&&) = default;

  const Taint& sources() const {
    return sources_;
  }

  const Taint& sinks() const {
    return sinks_;
  }

  const Rule* MT_NULLABLE rule() const {
    return rule_;
  }

  const HandleSet new_handles() const {
    return new_handles_;
  }

  const Position* MT_NULLABLE position() const {
    return position_;
  }

  static Issue bottom() {
    return Issue();
  }

  static Issue top() {
    mt_unreachable(); // Not implemented.
  }

  bool is_bottom() const override {
    return sources_.is_bottom() || sinks_.is_bottom() || rule_ == nullptr ||
        position_ == nullptr;
  }

  bool is_top() const override {
    return false;
  }

  void set_to_bottom() override {
    sources_.set_to_bottom();
    sinks_.set_to_bottom();
    rule_ = nullptr;
    position_ = nullptr;
  }

  void set_to_top() override {
    mt_unreachable(); // Not implemented.
  }

  bool leq(const Issue& other) const override;

  bool equals(const Issue& other) const override;

  void join_with(const Issue& other) override;

  void widen_with(const Issue& other) override;

  void meet_with(const Issue& other) override;

  void narrow_with(const Issue& other) override;

  void filter_sources(
      const std::function<
          bool(const Method* MT_NULLABLE, const AccessPath&, const Kind*)>&
          predicate);

  void filter_sinks(
      const std::function<
          bool(const Method* MT_NULLABLE, const AccessPath&, const Kind*)>&
          predicate);

  FeatureMayAlwaysSet features() const;

  Json::Value to_json() const;

  // Describe how to join issues together in `IssueSet`.
  struct GroupEqual {
    bool operator()(const Issue& left, const Issue& right) const;
  };

  // Describe how to join issues together in `IssueSet`.
  struct GroupHash {
    std::size_t operator()(const Issue& issue) const;
  };

  friend std::ostream& operator<<(std::ostream& out, const Issue& issue);

 private:
  Taint sources_;
  Taint sinks_;
  const Rule* MT_NULLABLE rule_;
  HandleSet new_handles_;
  const Position* MT_NULLABLE position_;
};

} // namespace marianatrench

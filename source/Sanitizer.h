/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <AbstractDomain.h>
#include <PatriciaTreeSetAbstractDomain.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Kind.h>

namespace marianatrench {

enum class SanitizerKind { Sources, Sinks, Propagations };

using KindSetAbstractDomain =
    sparta::PatriciaTreeSetAbstractDomain<const Kind*>;

/**
 * Represents a sanitizer for specific flows through a method.
 *
 * `sanitizer_kind` is either Sources (sanitize all sources flowing out of
 * the method), Sinks (sanitize all flows into sinks rechable within the
 * method) or Propagations (sanitize propagations from one port of the
 * method to another).
 *
 * `kinds` represents the kinds for which to sanitize flows. Top will
 * sanitize all flows, regardless of kind and bottom will not sanitize any
 * flows.
 *
 */
class Sanitizer final : public sparta::AbstractDomain<Sanitizer> {
 public:
  // Default constructor for sparta. Returns the Bottom Sanitizer
  Sanitizer()
      : sanitizer_kind_(SanitizerKind::Sources),
        kinds_(KindSetAbstractDomain::bottom()) {}
  Sanitizer(
      const SanitizerKind& sanitizer_kind,
      const KindSetAbstractDomain& kinds);

  Sanitizer(const Sanitizer&) = default;
  Sanitizer(Sanitizer&&) = default;
  Sanitizer& operator=(const Sanitizer&) = default;
  Sanitizer& operator=(Sanitizer&&) = default;

  static Sanitizer bottom() {
    return Sanitizer();
  }

  static Sanitizer top() {
    mt_unreachable(); // Not implemented.
  }

  bool is_bottom() const override {
    return kinds_.is_bottom();
  }

  bool is_top() const override {
    return false;
  }

  void set_to_bottom() override {
    kinds_.set_to_bottom();
  }

  void set_to_top() override {
    mt_unreachable(); // Not implemented.
  }

  bool leq(const Sanitizer& other) const override;

  bool equals(const Sanitizer& other) const override;

  void join_with(const Sanitizer& other) override;

  void widen_with(const Sanitizer& other) override;

  void meet_with(const Sanitizer& other) override;

  void narrow_with(const Sanitizer& other) override;

  SanitizerKind sanitizer_kind() const {
    return sanitizer_kind_;
  }

  const KindSetAbstractDomain& kinds() const {
    return kinds_;
  }

  friend std::ostream& operator<<(
      std::ostream& out,
      const Sanitizer& sanitizer);

  static const Sanitizer from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

  // Describe how to join sanitizers together in `SanitizerSet`.
  struct GroupEqual {
    bool operator()(const Sanitizer& left, const Sanitizer& right) const;
  };
  struct GroupHash {
    std::size_t operator()(const Sanitizer& frame) const;
  };

 private:
  SanitizerKind sanitizer_kind_;
  KindSetAbstractDomain kinds_;
};

} // namespace marianatrench

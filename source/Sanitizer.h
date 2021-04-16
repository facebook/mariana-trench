/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_set>

#include <AbstractDomain.h>
#include <PatriciaTreeSetAbstractDomain.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Kind.h>

namespace marianatrench {

enum class SanitizerKind { Sources, Sinks, Propagations };

using KindSetAbstractDomain =
    sparta::PatriciaTreeSetAbstractDomain<const Kind*>;

/**
 * Represents a sanitizer for specific flows through a method.
 *
 * `sanitizer_kind` is either Sources (sanitize all sources flowing out of the
 * method), Sinks (sanitize all flows into sinks rechable within the method) or
 * Propagations (sanitize propagations from one port of the method to another).
 *
 * `source_kinds` and `sinks_kinds` represent the kinds for which to sanitize
 * flows. Top will sanitize all flows, regardless of kind and bottom will not
 * sanitize any flows
 *
 */
class Sanitizer final : public sparta::AbstractDomain<Sanitizer> {
 public:
  // Default constructor for sparta. Returns the Bottom Sanitizer
  Sanitizer()
      : sanitizer_kind_(SanitizerKind::Sources),
        source_kinds_(KindSetAbstractDomain::bottom()),
        sink_kinds_(KindSetAbstractDomain::bottom()) {}
  Sanitizer(
      const SanitizerKind& sanitizer_kind,
      const KindSetAbstractDomain& source_kinds,
      const KindSetAbstractDomain& sink_kinds);

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
    return source_kinds_.is_bottom() && sink_kinds_.is_bottom();
  }

  bool is_top() const override {
    return false;
  }

  void set_to_bottom() override {
    source_kinds_.set_to_bottom();
    sink_kinds_.set_to_bottom();
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

  friend std::ostream& operator<<(
      std::ostream& out,
      const Sanitizer& sanitizer);

 private:
  SanitizerKind sanitizer_kind_;
  KindSetAbstractDomain source_kinds_;
  KindSetAbstractDomain sink_kinds_;
};

} // namespace marianatrench

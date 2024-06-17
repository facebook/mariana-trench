/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sparta/AbstractDomain.h>
#include <sparta/PatriciaTreeSetAbstractDomain.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/SanitizeTransform.h>
#include <mariana-trench/TransformList.h>

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

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Sanitizer)

  static Sanitizer bottom() {
    return Sanitizer();
  }

  static Sanitizer top() {
    mt_unreachable(); // Not implemented.
  }

  bool is_bottom() const {
    return kinds_.is_bottom();
  }

  bool is_top() const {
    return false;
  }

  void set_to_bottom() {
    kinds_.set_to_bottom();
  }

  void set_to_top() {
    mt_unreachable(); // Not implemented.
  }

  bool leq(const Sanitizer& other) const;

  bool equals(const Sanitizer& other) const;

  void join_with(const Sanitizer& other);

  void widen_with(const Sanitizer& other);

  void meet_with(const Sanitizer& other);

  void narrow_with(const Sanitizer& other);

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

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <limits>

#include <AbstractDomain.h>

#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

/* A scalar abstract domain, where bottom is max_int and 0 is top. */
class ScalarAbstractDomain final
    : public sparta::AbstractDomain<ScalarAbstractDomain> {
 public:
  using IntType = std::uint32_t;

  enum class Enum : IntType {
    Bottom = std::numeric_limits<IntType>::max(),
    Max = std::numeric_limits<IntType>::max() - 1u,
    Zero = 0u,
    Top = 0u,
  };

 public:
  /* Create the bottom element. */
  ScalarAbstractDomain() : value_(static_cast<IntType>(Enum::Bottom)) {}

  explicit ScalarAbstractDomain(IntType value) : value_(value) {}

  explicit ScalarAbstractDomain(Enum value)
      : value_(static_cast<IntType>(value)) {}

  ScalarAbstractDomain& operator=(IntType value) {
    value_ = value;
    return *this;
  }

  ScalarAbstractDomain& operator=(Enum value) {
    value_ = static_cast<IntType>(value);
    return *this;
  }

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ScalarAbstractDomain)

  IntType value() const {
    return value_;
  }

  static ScalarAbstractDomain bottom() {
    return ScalarAbstractDomain(Enum::Bottom);
  }

  static ScalarAbstractDomain top() {
    return ScalarAbstractDomain(Enum::Top);
  }

  bool is_bottom() const override {
    return value_ == static_cast<IntType>(Enum::Bottom);
  }

  bool is_top() const override {
    return value_ == static_cast<IntType>(Enum::Top);
  }

  void set_to_bottom() override {
    value_ = static_cast<IntType>(Enum::Bottom);
  }

  void set_to_top() override {
    value_ = static_cast<IntType>(Enum::Top);
  }

  bool leq(const ScalarAbstractDomain& other) const override {
    return value_ >= other.value_;
  }

  bool equals(const ScalarAbstractDomain& other) const override {
    return value_ == other.value_;
  }

  void join_with(const ScalarAbstractDomain& other) override {
    value_ = std::min(value_, other.value_);
  }

  void widen_with(const ScalarAbstractDomain& other) override {
    this->join_with(other);
  }

  void meet_with(const ScalarAbstractDomain& other) override {
    value_ = std::max(value_, other.value_);
  }

  void narrow_with(const ScalarAbstractDomain& other) override {
    this->meet_with(other);
  }

  void difference_with(const ScalarAbstractDomain& other) {
    if (this->leq(other)) {
      this->set_to_bottom();
    }
  }

  friend std::ostream& operator<<(
      std::ostream& out,
      const ScalarAbstractDomain& value) {
    if (value.is_bottom()) {
      out << "_|_";
    } else {
      out << value.value_;
    }
    return out;
  }

 private:
  IntType value_;
};

} // namespace marianatrench

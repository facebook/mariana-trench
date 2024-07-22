/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

#include <sparta/AbstractDomain.h>

#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

namespace scalar_impl {

using IntType = std::uint32_t;

struct ScalarTopIsZero final {
  enum class Enum : IntType {
    Bottom = std::numeric_limits<IntType>::max(),
    Max = std::numeric_limits<IntType>::max() - 1u,
    Zero = 0u,
    Top = 0u,
  };

  static bool leq(IntType left, IntType right) {
    return left >= right;
  }

  static IntType join_with(IntType left, IntType right) {
    return std::min(left, right);
  }

  static IntType meet_with(IntType left, IntType right) {
    return std::max(left, right);
  }
};

struct ScalarBottomIsZero final {
  enum class Enum : IntType {
    Top = std::numeric_limits<IntType>::max(),
    Max = std::numeric_limits<IntType>::max() - 1u,
    Zero = 0u,
    Bottom = 0u,
  };

  static bool leq(IntType left, IntType right) {
    return left <= right;
  }

  static IntType join_with(IntType left, IntType right) {
    return std::max(left, right);
  }

  static IntType meet_with(IntType left, IntType right) {
    return std::min(left, right);
  }
};

} // namespace scalar_impl

#define MT_ENUM_HAS_MEMBER(Enum, Member)                               \
  template <typename U>                                                \
  static auto has_##Member(U*)->decltype(U::Member, std::true_type{}); \
                                                                       \
  template <typename>                                                  \
  static auto has_##Member(...)->std::false_type;                      \
                                                                       \
  static_assert(decltype(has_##Member<typename Enum>(0))::value);

template <typename ScalarValue>
class ScalarAbstractDomainScaffolding final
    : public sparta::AbstractDomain<
          ScalarAbstractDomainScaffolding<ScalarValue>> {
 public:
  using IntType = scalar_impl::IntType;

 private:
  static_assert(
      std::is_same_v<
          decltype(ScalarValue::join_with(
              std::declval<IntType>(),
              std::declval<IntType>())),
          IntType>,
      "IntType ScalarValue::join_with(IntType,IntType) does not exist");
  static_assert(
      std::is_same_v<
          decltype(ScalarValue::meet_with(
              std::declval<IntType>(),
              std::declval<IntType>())),
          IntType>,
      "IntType ScalarValue::meet_with(IntType,IntType) does not exist");
  static_assert(
      std::is_same_v<
          decltype(ScalarValue::leq(
              std::declval<IntType>(),
              std::declval<IntType>())),
          bool>,
      "bool ScalarValue::leq(IntType,IntType) does not exist");

  static_assert(std::is_enum_v<typename ScalarValue::Enum> == true);
  static_assert(std::is_same_v<
                std::underlying_type_t<typename ScalarValue::Enum>,
                IntType>);

  MT_ENUM_HAS_MEMBER(ScalarValue::Enum, Top)
  MT_ENUM_HAS_MEMBER(ScalarValue::Enum, Bottom)
  MT_ENUM_HAS_MEMBER(ScalarValue::Enum, Zero)
  MT_ENUM_HAS_MEMBER(ScalarValue::Enum, Max)

 public:
  using Enum = typename ScalarValue::Enum;

 public:
  /* Create the bottom element. */
  ScalarAbstractDomainScaffolding()
      : value_(static_cast<IntType>(Enum::Bottom)) {}

  explicit ScalarAbstractDomainScaffolding(IntType value) : value_(value) {}

  explicit ScalarAbstractDomainScaffolding(Enum value)
      : value_(static_cast<IntType>(value)) {}

  ScalarAbstractDomainScaffolding& operator=(IntType value) {
    value_ = value;
    return *this;
  }

  ScalarAbstractDomainScaffolding& operator=(Enum value) {
    value_ = static_cast<IntType>(value);
    return *this;
  }

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(
      ScalarAbstractDomainScaffolding)

  IntType value() const {
    return value_;
  }

  static ScalarAbstractDomainScaffolding bottom() {
    return ScalarAbstractDomainScaffolding(Enum::Bottom);
  }

  static ScalarAbstractDomainScaffolding top() {
    return ScalarAbstractDomainScaffolding(Enum::Top);
  }

  bool is_bottom() const {
    return value_ == static_cast<IntType>(Enum::Bottom);
  }

  bool is_top() const {
    return value_ == static_cast<IntType>(Enum::Top);
  }

  void set_to_bottom() {
    value_ = static_cast<IntType>(Enum::Bottom);
  }

  void set_to_top() {
    value_ = static_cast<IntType>(Enum::Top);
  }

  bool leq(const ScalarAbstractDomainScaffolding& other) const {
    return ScalarValue::leq(value_, other.value_);
  }

  bool equals(const ScalarAbstractDomainScaffolding& other) const {
    return value_ == other.value_;
  }

  void join_with(const ScalarAbstractDomainScaffolding& other) {
    value_ = ScalarValue::join_with(value_, other.value_);
  }

  void widen_with(const ScalarAbstractDomainScaffolding& other) {
    this->join_with(other);
  }

  void meet_with(const ScalarAbstractDomainScaffolding& other) {
    value_ = ScalarValue::meet_with(value_, other.value_);
  }

  void narrow_with(const ScalarAbstractDomainScaffolding& other) {
    this->meet_with(other);
  }

  void difference_with(const ScalarAbstractDomainScaffolding& other) {
    if (this->leq(other)) {
      this->set_to_bottom();
    }
  }

  friend std::ostream& operator<<(
      std::ostream& out,
      const ScalarAbstractDomainScaffolding& value) {
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

#undef MT_ENUM_HAS_MEMBER

} // namespace marianatrench

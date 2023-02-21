/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <initializer_list>
#include <type_traits>

#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

/**
 * A set of flags.
 *
 * `Flags<Enum>` can be used to store an OR-combination of enum values, where
 * `Enum` is an enum class type. `Enum` underlying values must be a power of 2.
 */
template <typename Enum>
class Flags final {
  static_assert(std::is_enum_v<Enum>, "Enum must be an enumeration type");
  static_assert(
      std::is_unsigned_v<std::underlying_type_t<Enum>>,
      "The underlying type of Enum must be an unsigned arithmetic type");

 public:
  using EnumType = Enum;

 private:
  using IntT = std::underlying_type_t<Enum>;

 public:
  Flags() = default;

  /* implicit */ Flags(Enum flag) : value_(static_cast<IntT>(flag)) {}

  /* implicit */ Flags(std::initializer_list<Enum> flags) : value_(0) {
    for (auto flag : flags) {
      value_ |= static_cast<IntT>(flag);
    }
  }

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Flags)

  Flags& operator&=(Enum flag) {
    value_ &= static_cast<IntT>(flag);
    return *this;
  }

  Flags& operator&=(Flags flags) {
    value_ &= flags.value_;
    return *this;
  }

  Flags& operator|=(Enum flag) {
    value_ |= static_cast<IntT>(flag);
    return *this;
  }

  Flags& operator|=(Flags flags) {
    value_ |= flags.value_;
    return *this;
  }

  Flags& operator^=(Enum flag) {
    value_ ^= static_cast<IntT>(flag);
    return *this;
  }

  Flags& operator^=(Flags flags) {
    value_ ^= flags.value_;
    return *this;
  }

  Flags operator&(Enum flag) const {
    return Flags(value_ & static_cast<IntT>(flag));
  }

  Flags operator&(Flags flags) const {
    return Flags(value_ & flags.value_);
  }

  Flags operator|(Enum flag) const {
    return Flags(value_ | static_cast<IntT>(flag));
  }

  Flags operator|(Flags flags) const {
    return Flags(value_ | flags.value_);
  }

  Flags operator^(Enum flag) const {
    return Flags(value_ ^ static_cast<IntT>(flag));
  }

  Flags operator^(Flags flags) const {
    return Flags(value_ ^ flags.value_);
  }

  Flags operator~() const {
    return Flags(~value_);
  }

  explicit operator bool() const {
    return value_ != 0;
  }

  bool operator!() const {
    return value_ == 0;
  }

  bool operator==(Flags flags) const {
    return value_ == flags.value_;
  }

  bool test(Enum flag) const {
    if (static_cast<IntT>(flag) == 0) {
      return value_ == 0;
    } else {
      return (value_ & static_cast<IntT>(flag)) == static_cast<IntT>(flag);
    }
  }

  Flags& set(Enum flag, bool on = true) {
    if (on) {
      value_ |= static_cast<IntT>(flag);
    } else {
      value_ &= ~static_cast<IntT>(flag);
    }
    return *this;
  }

  bool empty() const {
    return value_ == 0;
  }

  void clear() {
    value_ = 0;
  }

  bool is_subset_of(Flags flags) const {
    return (value_ | flags.value_) == flags.value_;
  }

 private:
  explicit Flags(IntT value) : value_(value) {}

 private:
  IntT value_ = 0;
};

} // namespace marianatrench

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

#include <mariana-trench/Assert.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

namespace {

/**
 * Computes the log2 of a compile time constant `N`.
 */
template <std::size_t N>
struct ConstantLog2
    : std::integral_constant<std::size_t, ConstantLog2<N / 2>::value + 1> {};
template <>
struct ConstantLog2<1> : std::integral_constant<std::size_t, 0> {};

/**
 * Helper class that performs bit manipulations for PointerIntPair
 */
template <typename PointerType, typename IntType, unsigned IntBits>
class BitOperations {
  static constexpr int NumberOfLowBitsAvailable =
      ConstantLog2<alignof(PointerType)>::value;

  static_assert(
      NumberOfLowBitsAvailable < std::numeric_limits<uintptr_t>::digits,
      "cannot use a pointer type that has all bits free");
  static_assert(
      IntBits <= NumberOfLowBitsAvailable,
      "PointerIntPair with integer size too large for pointer");
  static_assert(sizeof(PointerType) == sizeof(uintptr_t));

  // Mask for the bits of the pointer.
  static constexpr uintptr_t PointerBitMask =
      ~(uintptr_t)(((intptr_t)1 << IntBits) - 1);

  // Mask for the bits of the int type.
  static constexpr uintptr_t IntMask =
      (uintptr_t)(((intptr_t)1 << IntBits) - 1);

 public:
  /**
   * Retrieve only the high-bits used for pointer value.
   */
  static PointerType MT_NULLABLE get_pointer(uintptr_t value) {
    return reinterpret_cast<PointerType>(value & PointerBitMask);
  }

  /**
   * Retrieve only the low-bits used for int value.
   */
  static IntType get_int(uintptr_t value) {
    uintptr_t int_value = value & IntMask;
    return static_cast<IntType>(int_value);
  }

  /**
   * Only update the int bits while preserving the high-bits used for pointer
   * value.
   */
  static uintptr_t update_int(uintptr_t value, IntType new_int) {
    uintptr_t int_value = static_cast<uintptr_t>(new_int);
    mt_assert((int_value & ~IntMask) == 0 && "Integer too large for field");

    return (value & ~IntMask) | int_value;
  }

  /**
   * Only update the pointer bits while preserving the low-bits used for int
   * value.
   */
  static uintptr_t update_pointer(uintptr_t value, PointerType new_pointer) {
    uintptr_t pointer = reinterpret_cast<uintptr_t>(new_pointer);
    mt_assert(
        (pointer & ~PointerBitMask) == 0 &&
        "Pointer is not sufficiently aligned");
    return pointer | (value & ~PointerBitMask);
  }
};

} // namespace

/**
 * PointerIntPair<PointerType, IntBits, IntType> class can be used to store a
 * pair of pointer and int values, where:
 *   - `PointerType` can be any pointer
 *   - `IntType` can be any int class type whose underlying type must be an
 *      unsigned int type.
 *   - `IntBits` is the number of low bits that is used to store `IntType`
 *      using bitmangling. This can only be done for small integers, and the
 *      maximum number of low bits available for this is computed as
 *      `NumberOfLowBitsAvailable` (typically upto 3 bits).
 */
template <typename PointerType, unsigned IntBits, typename IntType = unsigned>
class PointerIntPair final {
  static_assert(
      std::is_pointer_v<PointerType>,
      "The PointerType must be a pointer type");
  static_assert(
      std::is_unsigned_v<IntType>,
      "The IntType must be an unsigned arithmetic type");

 private:
  using Operations = BitOperations<PointerType, IntType, IntBits>;

 public:
  constexpr PointerIntPair() = default;

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(PointerIntPair)

  /**
   * Fill `value_` with just the `pointer_value`. The last `IntBits` bits will
   * be zero.
   */
  explicit PointerIntPair(PointerType pointer_value) {
    value_ = Operations::update_pointer(0, pointer_value);
  }

  /**
   * Fill `value_` with the aggregated value of pointer_value and int_value
   */
  PointerIntPair(PointerType pointer_value, IntType int_value) {
    set_pointer_and_int(pointer_value, int_value);
  }

  PointerType MT_NULLABLE get_pointer() const {
    return Operations::get_pointer(value_);
  }

  IntType get_int() const {
    return Operations::get_int(value_);
  }

  uintptr_t encode() const {
    return value_;
  }

  void set_pointer(PointerType pointer_value) {
    value_ = Operations::update_pointer(value_, pointer_value);
  }

  void set_int(IntType int_value) {
    value_ = Operations::update_int(value_, int_value);
  }

  void set_pointer_and_int(PointerType pointer_value, IntType int_value) {
    value_ = Operations::update_int(
        Operations::update_pointer(0, pointer_value), int_value);
  }

  bool operator==(const PointerIntPair& other) const {
    return value_ == other.value_;
  }

  bool operator!=(const PointerIntPair& other) const {
    return value_ != other.value_;
  }

 private:
  // value_ stores the pointer with the last IntBits bits containing the
  // int type data. Do not dereference directly.
  uintptr_t value_ = 0;
};

}; // namespace marianatrench

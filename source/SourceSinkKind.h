/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ostream>
#include <string>

#include <json/json.h>
#include <sparta/PatriciaTreeKeyTrait.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/PointerIntPair.h>

namespace marianatrench {

// Forward delaration of TransformDirection
namespace transforms {
enum class TransformDirection;
} // namespace transforms

// Forward declaration of SanitizerKind
enum class SanitizerKind;

/**
 * A wrapper around a PointerIntPair of Kind* and source/sink encoding.
 * This is used by transform-based sanitizers.
 */
class SourceSinkKind final {
 private:
  using KindPair = PointerIntPair<const Kind*, 2, unsigned>;

  enum class Encoding : unsigned {
    Source = 0b01,
    Sink = 0b10,
  };

  explicit SourceSinkKind(const Kind* kind, Encoding encoding) {
    value_ = KindPair(kind, static_cast<unsigned>(encoding));
  }

 public:
  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(SourceSinkKind)

  bool operator==(const SourceSinkKind& other) const {
    return value_.encode() == other.value_.encode();
  }

  bool operator<(const SourceSinkKind& other) const {
    return value_.encode() < other.value_.encode();
  }

  const Kind* kind() const {
    const Kind* kind = value_.get_pointer();
    mt_assert(kind != nullptr);
    return kind;
  }

  std::size_t hash() const {
    return value_.encode();
  }

  bool is_source() const {
    return value_.get_int() == static_cast<unsigned>(Encoding::Source);
  }

  bool is_sink() const {
    return value_.get_int() == static_cast<unsigned>(Encoding::Sink);
  }

  static SourceSinkKind source(const Kind* kind) {
    return SourceSinkKind(kind, Encoding::Source);
  }

  static SourceSinkKind sink(const Kind* kind) {
    return SourceSinkKind(kind, Encoding::Sink);
  }

  static SourceSinkKind from_transform_direction(
      const Kind* kind,
      transforms::TransformDirection direction);

  static SourceSinkKind from_trace_string(
      const std::string& value,
      Context& context,
      SanitizerKind sanitizer_kind);

  static SourceSinkKind from_config_json(
      const Json::Value& value,
      Context& context,
      SanitizerKind sanitizer_kind);

  std::string to_trace_string(SanitizerKind sanitizer_kind) const;
  Json::Value to_json(SanitizerKind sanitizer_kind) const;

 private:
  friend std::size_t hash_value(const SourceSinkKind& pair);

  KindPair value_;
};

// For hash in boost
std::size_t hash_value(const SourceSinkKind& pair);

std::ostream& operator<<(std::ostream& out, const SourceSinkKind& pair);

} // namespace marianatrench

// SourceSinkKind does not have inheritance and has a single PointerIntPair,
// this means its layout would be the same as the underlying PointerIntPair,
// which is a uintptr_t we can safely reinterpret_cast into.
template <>
struct sparta::PatriciaTreeKeyTrait<marianatrench::SourceSinkKind> {
  using IntegerType = uintptr_t;
};

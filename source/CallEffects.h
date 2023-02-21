/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/TaintTree.h>

namespace marianatrench {

/**
 * Represents the type of call effect.
 */
class CallEffect final {
 public:
  using IntegerEncoding = std::uint32_t;

  // TODO(T131380009) Support sapp traces for via-dependency-graph traversal.
  enum class Kind : IntegerEncoding {
    CALL_CHAIN = 0,
  };

 private:
  explicit CallEffect(IntegerEncoding value) : value_(value) {}

 public:
  /* Default constructor required by sparta, do not use. */
  explicit CallEffect()
      : value_(static_cast<IntegerEncoding>(Kind::CALL_CHAIN)) {}

  explicit CallEffect(Kind kind) : value_(static_cast<IntegerEncoding>(kind)) {}

  bool operator==(const CallEffect& other) const {
    return value_ == other.value_;
  }

  bool operator!=(const CallEffect& other) const {
    return value_ != other.value_;
  }

  Kind kind() const {
    return static_cast<Kind>(value_);
  }

  IntegerEncoding encode() const {
    return value_;
  }

  std::string to_string() const;

  AccessPath access_path() const;

  static CallEffect from_json(const Json::Value& value);
  Json::Value to_json() const;

  static CallEffect decode(IntegerEncoding value) {
    return CallEffect(value);
  }

 private:
  friend std::ostream& operator<<(std::ostream& out, const CallEffect& effect);

 private:
  // Note `PatriciaTreeAbstractPartition` relies on this encoding.
  IntegerEncoding value_;
};

} // namespace marianatrench

template <>
struct std::hash<marianatrench::CallEffect> {
  std::size_t operator()(const marianatrench::CallEffect& effect) const {
    return effect.encode();
  }
};

namespace marianatrench {

/**
 * This represents a map from calleffects to taint.
 */
class CallEffectsAbstractDomain final
    : public sparta::AbstractDomain<CallEffectsAbstractDomain> {
 private:
  using Map = sparta::
      PatriciaTreeMapAbstractPartition<CallEffect::IntegerEncoding, Taint>;

  // Since the patricia tree map key needs to be an integer, we need this to
  // transform the key back to a `CallEffect` when iterating on the map.
  struct ExposeBinding {
    const std::pair<CallEffect, Taint>& operator()(
        const std::pair<CallEffect::IntegerEncoding, Taint>& pair) const {
      // This is safe as long as `CallEffect` stores `IntegerEncoding`
      // internally.
      return *reinterpret_cast<const std::pair<CallEffect, Taint>*>(&pair);
    }
  };

 public:
  // C++ container concept member types
  using key_type = CallEffect;
  using mapped_type = Taint;
  using value_type = std::pair<CallEffect, Taint>;
  using iterator =
      boost::transform_iterator<ExposeBinding, typename Map::MapType::iterator>;
  using const_iterator = iterator;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using const_reference = const value_type&;
  using const_pointer = const value_type*;

 private:
  // Safety checks of `boost::transform_iterator`.
  static_assert(std::is_same_v<typename iterator::value_type, value_type>);
  static_assert(
      std::is_same_v<typename iterator::difference_type, difference_type>);
  static_assert(std::is_same_v<typename iterator::reference, const_reference>);
  static_assert(std::is_same_v<typename iterator::pointer, const_pointer>);

 private:
  explicit CallEffectsAbstractDomain(Map map) : map_(std::move(map)) {}

 public:
  CallEffectsAbstractDomain() = default;

  CallEffectsAbstractDomain(const CallEffectsAbstractDomain&) = default;
  CallEffectsAbstractDomain(CallEffectsAbstractDomain&&) = default;
  CallEffectsAbstractDomain& operator=(const CallEffectsAbstractDomain&) =
      default;
  CallEffectsAbstractDomain& operator=(CallEffectsAbstractDomain&&) = default;
  ~CallEffectsAbstractDomain() = default;

  // Map interface
  std::size_t size() const {
    return map_.size();
  }

  iterator begin() const {
    return boost::make_transform_iterator(
        map_.bindings().begin(), ExposeBinding());
  }

  iterator end() const {
    return boost::make_transform_iterator(
        map_.bindings().end(), ExposeBinding());
  }

  INCLUDE_ABSTRACT_DOMAIN_METHODS(CallEffectsAbstractDomain, Map, map_)

  const Taint& read(CallEffect effect) const;

  void visit(
      std::function<void(const CallEffect&, const Taint&)> visitor) const;

  /* Apply the given function on all elements. */
  void map(const std::function<void(Taint&)>& f);

  /* Performs a weak update. */
  void write(const CallEffect& effect, Taint taint);

  friend std::ostream& operator<<(
      std::ostream& out,
      const CallEffectsAbstractDomain& effects);

 private:
  Map map_;
};

} // namespace marianatrench

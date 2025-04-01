/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ConcurrentContainers.h>

namespace marianatrench {

/**
 * A thread-safe map from `Key` to `std::unique_pointer<const Value>`.
 * `ConcurrentMap` can only return by value. `UniquePointerConcurrentMap` can
 * be used instead to avoid copying on lookup.
 */
template <typename Key, typename Value>
class UniquePointerConcurrentMap final {
 private:
  using Map = ConcurrentMap<Key, Value*>;

 public:
  // C++ container concept member types
  using iterator = typename Map::const_iterator;
  using const_iterator = iterator;
  using key_type = Key;
  using mapped_type = const Value*;
  using value_type = std::pair<const key_type, mapped_type>;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;
  using const_reference = const value_type&;
  using const_pointer = const value_type*;

 public:
  UniquePointerConcurrentMap() = default;

  UniquePointerConcurrentMap(const UniquePointerConcurrentMap&) = delete;

  UniquePointerConcurrentMap(UniquePointerConcurrentMap&&) = delete;

  UniquePointerConcurrentMap& operator=(const UniquePointerConcurrentMap&) =
      delete;

  UniquePointerConcurrentMap& operator=(UniquePointerConcurrentMap&&) = delete;

  ~UniquePointerConcurrentMap() {
    // ConcurrentMap can only return by value, hence we need to store a raw
    // pointer and use delete here.
    for (const auto& entry : map_) {
      delete entry.second;
    }
  }

  const Value* at(const Key& key) const {
    return map_.at(key);
  }

  const Value* get(const Key& key, Value* default_value) const {
    return map_.get(key, default_value);
  }

  // Returns a non-const Value* which may be modified. This is not thread-safe.
  Value* get_unsafe(const Key& key) const {
    auto* result = map_.get_unsafe(key);
    return result == nullptr ? nullptr : *result;
  }

  bool emplace(const Key& key, std::unique_ptr<Value> value) {
    bool inserted = map_.emplace(key, value.get());

    if (inserted) {
      value.release();
    }

    return inserted;
  }

  /**
   * Iterating on the container while calling `emplace` concurrently is unsafe.
   */

  iterator begin() const {
    return map_.cbegin();
  }

  iterator end() const {
    return map_.cend();
  }

 private:
  Map map_;
};

} // namespace marianatrench

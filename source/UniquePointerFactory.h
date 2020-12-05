/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ConcurrentContainers.h>

#include <mariana-trench/Compiler.h>

namespace marianatrench {

/**
 * A thread-safe factory that returns a unique pointer for a given key.
 */
template <typename Key, typename Value>
class UniquePointerFactory final {
 private:
  using Map = ConcurrentMap<Key, const Value*>;

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
  UniquePointerFactory() = default;

  UniquePointerFactory(const UniquePointerFactory&) = delete;

  UniquePointerFactory(UniquePointerFactory&&) = delete;

  UniquePointerFactory& operator=(const UniquePointerFactory&) = delete;

  UniquePointerFactory& operator=(UniquePointerFactory&&) = delete;

  ~UniquePointerFactory() {
    // ConcurrentMap can only return by value, hence we need to store a raw
    // pointer and use delete here.
    for (const auto& entry : map_) {
      delete entry.second;
    }
  }

  /* Get or create a unique pointer for the given key. */
  const Value* create(const Key& key) const {
    const Value* result = nullptr;
    map_.update(
        key, [&](const Key& /* key */, const Value*& pointer, bool exists) {
          // This is an atomic block.
          if (!exists) {
            pointer = new Value(key);
          }
          result = pointer;
        });
    return result;
  }

  /**
   * Get the unique pointer for the given key.
   *
   * Returns `nullptr` if the key does not exist in the factory.
   */
  const Value* MT_NULLABLE get(const Key& key) const {
    return map_.get(key, nullptr);
  }

  /**
   * Iterating on the container while calling `create` concurrently is unsafe.
   */

  iterator begin() const {
    return map_.cbegin();
  }

  iterator end() const {
    return map_.cend();
  }

 private:
  mutable Map map_;
};

} // namespace marianatrench

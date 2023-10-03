/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <boost/functional/hash.hpp>

namespace marianatrench {

// Within `marianatrench::`, use `std::hash` as the `boost::hash_value`.
template <typename T>
std::size_t hash_value(const T& value) {
  return std::hash<T>()(value);
}

} // namespace marianatrench

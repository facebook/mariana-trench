/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

namespace marianatrench {

template <typename... Ts>
struct TupleHash {
  std::size_t operator()(const std::tuple<Ts...>& tuple) const {
    return boost::hash_value(tuple);
  }
};

} // namespace marianatrench

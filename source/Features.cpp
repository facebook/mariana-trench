/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Features.h>

namespace marianatrench {

const Feature* Features::get(const std::string& data) const {
  return factory_.create(data);
}

} // namespace marianatrench

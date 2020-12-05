/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Kinds.h>

namespace marianatrench {

const Kind* Kinds::get(const std::string& name) const {
  return factory_.create(name);
}

const Kind* Kinds::artificial_source() {
  static const Kind kind("<ArtificialSource>");
  return &kind;
}

} // namespace marianatrench

// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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

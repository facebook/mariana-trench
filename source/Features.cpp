// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <mariana-trench/Features.h>

namespace marianatrench {

const Feature* Features::get(const std::string& data) const {
  return factory_.create(data);
}

} // namespace marianatrench

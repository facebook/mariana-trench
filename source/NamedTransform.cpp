/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/NamedTransform.h>
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {

void NamedTransform::show(std::ostream& out) const {
  out << name_;
}

const NamedTransform* NamedTransform::from_trace_string(
    const std::string& transform,
    Context& context) {
  return context.transforms_factory->create_transform(transform);
}

std::string NamedTransform::to_trace_string() const {
  return name_;
}

} // namespace marianatrench

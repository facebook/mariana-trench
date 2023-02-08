/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>

#include <mariana-trench/LocalArgumentKind.h>

namespace marianatrench {

void LocalArgumentKind::show(std::ostream& out) const {
  out << "LocalArgument(" << parameter_ << ")";
}

std::string LocalArgumentKind::to_trace_string() const {
  return fmt::format("LocalArgument({})", parameter_);
}

Root LocalArgumentKind::root() const {
  return Root(Root::Kind::Argument, parameter_);
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>
#include <re2/re2.h>

#include <mariana-trench/KindFactory.h>
#include <mariana-trench/LocalArgumentKind.h>

namespace marianatrench {

void LocalArgumentKind::show(std::ostream& out) const {
  out << "LocalArgument(" << parameter_ << ")";
}

std::string LocalArgumentKind::to_trace_string() const {
  return fmt::format("LocalArgument({})", parameter_);
}

const LocalArgumentKind* LocalArgumentKind::from_trace_string(
    const std::string& kind,
    Context& context) {
  int parameter_position = 0;
  static const re2::RE2 regex("^LocalArgument\\((\\d+)\\)$");
  if (re2::RE2::FullMatch(kind, regex, &parameter_position)) {
    return context.kind_factory->local_argument(parameter_position);
  }

  throw InvalidKindStringError(kind, "LocalArgument(parameter_position)");
}

Root LocalArgumentKind::root() const {
  return Root(Root::Kind::Argument, parameter_);
}

} // namespace marianatrench

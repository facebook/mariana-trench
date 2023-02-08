/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/LocalReturnKind.h>

namespace marianatrench {

void LocalReturnKind::show(std::ostream& out) const {
  out << "LocalReturn";
}

std::string LocalReturnKind::to_trace_string() const {
  return "LocalReturn";
}

Root LocalReturnKind::root() const {
  return Root(Root::Kind::Return);
}

} // namespace marianatrench

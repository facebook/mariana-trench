/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/TriggeredPartialKind.h>

namespace marianatrench {

void TriggeredPartialKind::show(std::ostream& out) const {
  out << "TriggeredPartial:" << label() << ":" << name();
}

} // namespace marianatrench

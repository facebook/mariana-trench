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
  out << "TriggeredPartial:" << partial_kind_->name() << ":"
      << partial_kind_->label() << ":" << rule_->code();
}

Json::Value TriggeredPartialKind::to_json() const {
  // This value must be the same as the partial kind's value for traces from a
  // frame with a triggered kind to connect to its callee frame with a partial
  // non-triggered kind. This string equality property is used by the UI.
  return partial_kind_->to_json();
}

} // namespace marianatrench

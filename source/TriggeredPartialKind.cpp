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
  out << "TriggeredPartial:" << partial_kind_->label() << ":"
      << partial_kind_->name();
}

Json::Value TriggeredPartialKind::to_json() const {
  // TODO(T66517244): Should this be { "name": name(), "label": label() }?
  // Also check what is needed for traces to work..
  auto value = "TriggeredPartial:" + partial_kind_->label() + ":" +
      partial_kind_->name();
  return Json::Value(value);
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Origin.h>

namespace marianatrench {

std::string MethodOrigin::to_string() const {
  return method_->show();
}

Json::Value MethodOrigin::to_json() const {
  return method_->to_json();
}

std::string FieldOrigin::to_string() const {
  return field_->show();
}

Json::Value FieldOrigin::to_json() const {
  return field_->to_json();
}

} // namespace marianatrench

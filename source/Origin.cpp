/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Origin.h>

namespace marianatrench {

std::string MethodOrigin::to_string() const {
  return "method=" + method_->show() + ",port=" + port_->to_string();
}

Json::Value MethodOrigin::to_json() const {
  auto value = Json::Value(Json::objectValue);
  value["method"] = method_->to_json();
  value["port"] = port_->to_json();
  return value;
}

std::string FieldOrigin::to_string() const {
  return field_->show();
}

Json::Value FieldOrigin::to_json() const {
  auto value = Json::Value(Json::objectValue);
  value["field"] = field_->to_json();
  return value;
}

std::string CrtexOrigin::to_string() const {
  return canonical_name_->str_copy();
}

Json::Value CrtexOrigin::to_json() const {
  auto value = Json::Value(Json::objectValue);
  value["canonical_name"] = canonical_name_->str_copy();
  value["port"] = port_->to_json();
  return value;
}

} // namespace marianatrench

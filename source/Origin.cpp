/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>

#include <mariana-trench/AccessPathFactory.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Origin.h>
#include <mariana-trench/OriginFactory.h>

namespace marianatrench {

const Origin* Origin::from_json(const Json::Value& value, Context& context) {
  JsonValidation::validate_object(value);
  if (value.isMember("method") && value.isMember("port")) {
    return context.origin_factory->method_origin(
        Method::from_json(value["method"], context),
        context.access_path_factory->get(AccessPath::from_json(value["port"])));
  } else if (value.isMember("field")) {
    return context.origin_factory->field_origin(
        Field::from_json(value["field"], context));
  } else if (value.isMember("canonical_name")) {
    return context.origin_factory->crtex_origin(
        JsonValidation::string(value, "canonical_name"),
        context.access_path_factory->get(AccessPath::from_json(value["port"])));
  } else if (value.isMember("method")) {
    return context.origin_factory->string_origin(
        JsonValidation::string(value, "method"));
  } else if (
      value.isMember("exploitability_root") && value.isMember("callee")) {
    return context.origin_factory->exploitability_origin(
        Method::from_json(value["exploitability_root"], context),
        JsonValidation::string(value, "callee"));
  }

  throw JsonValidationError(
      value,
      std::nullopt,
      "contains one of fields [method|field|canonical_name|exploitability_root]");
}

std::string MethodOrigin::to_string() const {
  return "method=" + method_->show() + ",port=" + port_->to_string();
}

std::optional<std::string> MethodOrigin::to_model_validator_string() const {
  return method_->show();
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

std::optional<std::string> FieldOrigin::to_model_validator_string() const {
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

std::optional<std::string> CrtexOrigin::to_model_validator_string() const {
  return std::nullopt;
}

Json::Value CrtexOrigin::to_json() const {
  auto value = Json::Value(Json::objectValue);
  value["canonical_name"] = canonical_name_->str_copy();
  value["port"] = port_->to_json();
  return value;
}

std::string StringOrigin::to_string() const {
  return name_->str_copy();
}

std::optional<std::string> StringOrigin::to_model_validator_string() const {
  return name_->str_copy();
}

Json::Value StringOrigin::to_json() const {
  auto value = Json::Value(Json::objectValue);
  // Rename key to something more descriptive. Using "method" for now to
  // remain compatible with the parser.
  value["method"] = name_->str_copy();
  return value;
}

std::string ExploitabilityOrigin::to_string() const {
  return fmt::format(
      "exploitability_root={},callee={}",
      exploitability_root_->show(),
      callee_->str());
}

std::optional<std::string> ExploitabilityOrigin::to_model_validator_string()
    const {
  return std::nullopt;
}

std::string ExploitabilityOrigin::issue_handle_callee() const {
  return fmt::format("{}:{}", exploitability_root_->show(), callee_->str());
}

Json::Value ExploitabilityOrigin::to_json() const {
  auto value = Json::Value(Json::objectValue);
  value["exploitability_root"] = exploitability_root_->to_json();
  value["callee"] = callee_->str_copy();
  return value;
}

} // namespace marianatrench

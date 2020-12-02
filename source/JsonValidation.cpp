// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <boost/algorithm/string/trim.hpp>
#include <fmt/format.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Redex.h>

namespace marianatrench {

namespace {

std::string invalid_argument_message(
    const Json::Value& value,
    const std::optional<std::string>& field,
    const std::string& expected) {
  auto field_information = field ? fmt::format(" for field `{}`", *field) : "";
  return fmt::format(
      "Error validating `{}`. Expected {}{}.",
      boost::algorithm::trim_copy(value.toStyledString()),
      expected,
      field_information);
}

} // namespace

JsonValidationError::JsonValidationError(
    const Json::Value& value,
    const std::optional<std::string>& field,
    const std::string& expected)
    : std::invalid_argument(invalid_argument_message(value, field, expected)) {}

void JsonValidation::validate_object(const Json::Value& value) {
  if (value.isNull() || !value.isObject()) {
    throw JsonValidationError(
        value, /* field */ std::nullopt, /* expected */ "non-null object");
  }
}

const Json::Value& JsonValidation::object(
    const Json::Value& value,
    const std::string& field) {
  const auto& attribute = value[field];
  if (attribute.isNull() || !attribute.isObject()) {
    throw JsonValidationError(value, field, /* expected */ "non-null object");
  }
  return attribute;
}

std::string JsonValidation::string(const Json::Value& value) {
  if (value.isNull() || !value.isString()) {
    throw JsonValidationError(
        value, /* field */ std::nullopt, /* expected */ "string");
  }
  return value.asString();
}

std::string JsonValidation::string(
    const Json::Value& value,
    const std::string& field) {
  validate_object(value);
  const auto& string = value[field];
  if (string.isNull() || !string.isString()) {
    throw JsonValidationError(value, field, /* expected */ "string");
  }
  return string.asString();
}

int JsonValidation::integer(const Json::Value& value) {
  if (value.isNull() || !value.isInt()) {
    throw JsonValidationError(
        value, /* field */ std::nullopt, /* expected */ "integer");
  }
  return value.asInt();
}

int JsonValidation::integer(
    const Json::Value& value,
    const std::string& field) {
  validate_object(value);
  const auto& integer = value[field];
  if (integer.isNull() || !integer.isInt()) {
    throw JsonValidationError(value, field, /* expected */ "integer");
  }
  return integer.asInt();
}

bool JsonValidation::boolean(const Json::Value& value) {
  if (value.isNull() || !value.isBool()) {
    throw JsonValidationError(
        value, /* field */ std::nullopt, /* expected */ "boolean");
  }
  return value.asBool();
}

bool JsonValidation::boolean(
    const Json::Value& value,
    const std::string& field) {
  validate_object(value);
  const auto& boolean = value[field];
  if (boolean.isNull() || !boolean.isBool()) {
    throw JsonValidationError(value, field, /* expected */ "boolean");
  }
  return boolean.asBool();
}

const Json::Value& JsonValidation::null_or_array(const Json::Value& value) {
  if (!value.isNull() && !value.isArray()) {
    throw JsonValidationError(
        value, /* field */ std::nullopt, /* expected */ "null or array");
  }
  return value;
}

const Json::Value& JsonValidation::null_or_array(
    const Json::Value& value,
    const std::string& field) {
  validate_object(value);
  const auto& null_or_array = value[field];
  if (!null_or_array.isNull() && !null_or_array.isArray()) {
    throw JsonValidationError(value, field, /* expected */ "null or array");
  }
  return null_or_array;
}

const Json::Value& JsonValidation::nonempty_array(
    const Json::Value& value,
    const std::string& field) {
  validate_object(value);
  const auto& nonempty_array = value[field];
  if (nonempty_array.isNull() || !nonempty_array.isArray() ||
      nonempty_array.empty()) {
    throw JsonValidationError(value, field, /* expected */ "non-empty array");
  }
  return nonempty_array;
}

const Json::Value& JsonValidation::object_or_string(
    const Json::Value& value,
    const std::string& field) {
  validate_object(value);
  const auto& attribute = value[field];
  if (attribute.isNull() || (!attribute.isObject() && !attribute.isString())) {
    throw JsonValidationError(value, field, /* expected */ "object or string");
  }
  return attribute;
}

DexType* JsonValidation::dex_type(const Json::Value& value) {
  auto type_name = JsonValidation::string(value);
  auto* type = redex::get_type(type_name);
  if (!type) {
    throw JsonValidationError(
        value,
        /* field */ std::nullopt,
        /* expected */ "existing type name");
  }
  return type;
}

DexType* JsonValidation::dex_type(
    const Json::Value& value,
    const std::string& field) {
  auto type_name = JsonValidation::string(value, field);
  auto* type = redex::get_type(type_name);
  if (!type) {
    throw JsonValidationError(
        value,
        field,
        /* expected */ "existing type name");
  }
  return type;
}

DexFieldRef* JsonValidation::dex_field(const Json::Value& value) {
  auto field_name = JsonValidation::string(value);
  auto* dex_field = redex::get_field(field_name);
  if (!dex_field) {
    throw JsonValidationError(
        value, /* field */ std::nullopt, /* expected */ "existing field name");
  }
  return dex_field;
}

DexFieldRef* JsonValidation::dex_field(
    const Json::Value& value,
    const std::string& field) {
  auto field_name = JsonValidation::string(value, field);
  auto* dex_field = redex::get_field(field_name);
  if (!dex_field) {
    throw JsonValidationError(
        value, field, /* expected */ "existing field name");
  }
  return dex_field;
}

// Parse a json file into a json value
Json::Value JsonValidation::parse_json_file(
    const boost::filesystem::path& path) {
  std::string content;
  boost::filesystem::load_string_file(path, content);

  Json::Value json;
  if (!Json::Reader().parse(content, json)) {
    throw std::invalid_argument(
        fmt::format("File `{}` is not valid json.", path.string()));
  }
  return json;
}

Json::Value JsonValidation::parse_json_file(const std::string& path) {
  return parse_json_file(boost::filesystem::path(path));
}

void JsonValidation::update_object(
    Json::Value& left,
    const Json::Value& right) {
  mt_assert(left.isObject());
  mt_assert(right.isObject());

  for (const auto& key : right.getMemberNames()) {
    left[key] = right[key];
  }
}

} // namespace marianatrench

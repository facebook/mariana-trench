/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <fmt/format.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/JsonReaderWriter.h>
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
      boost::algorithm::trim_copy(JsonWriter::to_styled_string(value)),
      expected,
      field_information);
}

} // namespace

JsonValidationError::JsonValidationError(
    const Json::Value& value,
    const std::optional<std::string>& field,
    const std::string& expected)
    : std::invalid_argument(invalid_argument_message(value, field, expected)) {}

void JsonValidation::validate_object(
    const Json::Value& value,
    const std::string& expected) {
  if (value.isNull() || !value.isObject()) {
    throw JsonValidationError(
        value, /* field */ std::nullopt, /* expected */ expected);
  }
}

void JsonValidation::validate_object(
    const Json::Value& value,
    const std::string& field,
    const std::string& expected) {
  if (value.isNull() || !value.isObject() || !value.isMember(field)) {
    throw JsonValidationError(
        value, /* field */ std::nullopt, /* expected */ expected);
  }
}

void JsonValidation::validate_object(const Json::Value& value) {
  validate_object(value, /* expected */ "non-null object");
}

const Json::Value& JsonValidation::object(
    const Json::Value& value,
    const std::string& field) {
  validate_object(
      value, field, fmt::format("non-null object with field `{}`", field));
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
  validate_object(
      value, fmt::format("non-null object with string field `{}`", field));
  const auto& string = value[field];
  if (string.isNull() || !string.isString()) {
    throw JsonValidationError(value, field, /* expected */ "string");
  }
  return string.asString();
}

std::optional<std::string> JsonValidation::optional_string(
    const Json::Value& value,
    const std::string& field) {
  validate_object(
      value, fmt::format("non-null object with string field `{}`", field));
  const auto& string = value[field];
  if (string.isNull()) {
    return std::nullopt;
  }
  if (!string.isString()) {
    throw JsonValidationError(value, field, /* expected */ "string");
  }
  return string.asString();
}

std::string JsonValidation::string_or_default(
    const Json::Value& value,
    const std::string& field,
    const std::string& default_value) {
  validate_object(
      value, fmt::format("non-null object with string field `{}`", field));
  const auto& string = value[field];
  if (string.isNull()) {
    return default_value;
  }
  if (!string.isString()) {
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
  validate_object(
      value,
      field,
      fmt::format("non-null object with integer field `{}`", field));
  const auto& integer = value[field];
  if (integer.isNull() || !integer.isInt()) {
    throw JsonValidationError(value, field, /* expected */ "integer");
  }
  return integer.asInt();
}

std::optional<int> JsonValidation::optional_integer(
    const Json::Value& value,
    const std::string& field) {
  validate_object(
      value, fmt::format("non-null object with integer field `{}`", field));
  const auto& integer = value[field];
  if (integer.isNull()) {
    return std::nullopt;
  }
  if (!integer.isInt()) {
    throw JsonValidationError(value, field, /* expected */ "integer");
  }
  return integer.asInt();
}

uint32_t JsonValidation::unsigned_integer(const Json::Value& value) {
  if (value.isNull() || !value.isUInt()) {
    throw JsonValidationError(
        value,
        /* field */ std::nullopt,
        /* expected */ "unsigned integer (32-bit)");
  }
  return value.asUInt();
}

uint32_t JsonValidation::unsigned_integer(
    const Json::Value& value,
    const std::string& field) {
  validate_object(
      value,
      fmt::format("non-null object with unsigned integer field `{}`", field));
  const auto& integer = value[field];
  if (integer.isNull() || !integer.isUInt()) {
    throw JsonValidationError(value, field, /* expected */ "unsigned integer");
  }
  return integer.asUInt();
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
  validate_object(
      value,
      field,
      fmt::format("non-null object with boolean field `{}`", field));
  const auto& boolean = value[field];
  if (boolean.isNull() || !boolean.isBool()) {
    throw JsonValidationError(value, field, /* expected */ "boolean");
  }
  return boolean.asBool();
}

bool JsonValidation::optional_boolean(
    const Json::Value& value,
    const std::string& field,
    bool default_value) {
  validate_object(
      value, fmt::format("non-null object with boolean field `{}`", field));
  const auto& boolean = value[field];
  if (boolean.isNull()) {
    return default_value;
  }
  if (!boolean.isBool()) {
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
  validate_object(
      value,
      fmt::format("non-null object with null or array field `{}`", field));
  if (!value.isMember(field)) {
    return Json::Value::nullSingleton();
  }

  const auto& null_or_array = value[field];
  if (!null_or_array.isNull() && !null_or_array.isArray()) {
    throw JsonValidationError(value, field, /* expected */ "null or array");
  }

  return null_or_array;
}

const Json::Value& JsonValidation::nonempty_array(const Json::Value& value) {
  if (value.isNull() || !value.isArray() || value.empty()) {
    throw JsonValidationError(
        value, /* field */ std::nullopt, /* expected */ "non-empty array");
  }
  return value;
}

const Json::Value& JsonValidation::nonempty_array(
    const Json::Value& value,
    const std::string& field) {
  validate_object(
      value,
      field,
      fmt::format("non-null object with non-empty array field `{}`", field));
  const auto& nonempty_array = value[field];
  if (nonempty_array.isNull() || !nonempty_array.isArray() ||
      nonempty_array.empty()) {
    throw JsonValidationError(value, field, /* expected */ "non-empty array");
  }
  return nonempty_array;
}

const Json::Value& JsonValidation::null_or_object(
    const Json::Value& value,
    const std::string& field) {
  validate_object(
      value,
      fmt::format("non-null object with null or object field `{}`", field));
  if (!value.isMember(field)) {
    return Json::Value::nullSingleton();
  }

  return object(value, field);
}

const Json::Value& JsonValidation::object_or_string(
    const Json::Value& value,
    const std::string& field) {
  validate_object(
      value,
      field,
      fmt::format("non-null object with object or string field `{}`", field));
  const auto& attribute = value[field];
  if (attribute.isNull() || (!attribute.isObject() && !attribute.isString())) {
    throw JsonValidationError(value, field, /* expected */ "object or string");
  }
  return attribute;
}

bool JsonValidation::has_field(
    const Json::Value& value,
    const std::string& field) {
  const auto& attribute = value[field];
  return !attribute.isNull();
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

void JsonValidation::check_unexpected_members(
    const Json::Value& value,
    const std::unordered_set<std::string>& valid_members) {
  validate_object(value);

  for (const std::string& member : value.getMemberNames()) {
    if (valid_members.find(member) == valid_members.end()) {
      throw JsonValidationError(
          value,
          /* field */ std::nullopt,
          /* expected */
          fmt::format(
              "fields {}, got `{}`",
              fmt::join(
                  boost::adaptors::transform(
                      valid_members,
                      [](const std::string& member) {
                        return fmt::format("`{}`", member);
                      }),
                  ", "),
              member));
    }
  }
}

} // namespace marianatrench

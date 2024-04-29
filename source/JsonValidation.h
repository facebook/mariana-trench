/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>
#include <string>
#include <unordered_set>

#include <json/json.h>

#include <DexClass.h>

namespace marianatrench {

class JsonValidationError : public std::invalid_argument {
 public:
  JsonValidationError(
      const Json::Value& value,
      const std::optional<std::string>& field,
      const std::string& expected);
};

class JsonValidation final {
 public:
  static constexpr std::size_t k_default_shard_limit = 10000;

  static void validate_object(const Json::Value& value);
  static void validate_object(
      const Json::Value& value,
      const std::string& expected);

  static const Json::Value& object(
      const Json::Value& value,
      const std::string& field);

  static std::string string(const Json::Value& value);
  static std::string string(const Json::Value& value, const std::string& field);

  static int integer(const Json::Value& value);
  static int integer(const Json::Value& value, const std::string& field);
  static std::optional<int> optional_integer(
      const Json::Value& value,
      const std::string& field);

  static uint32_t unsigned_integer(const Json::Value& value);
  static uint32_t unsigned_integer(
      const Json::Value& value,
      const std::string& field);

  static bool boolean(const Json::Value& value);
  static bool boolean(const Json::Value& value, const std::string& field);

  static const Json::Value& null_or_array(const Json::Value& value);
  static const Json::Value& null_or_array(
      const Json::Value& value,
      const std::string& field);

  static const Json::Value& nonempty_array(const Json::Value& value);
  static const Json::Value& nonempty_array(
      const Json::Value& value,
      const std::string& field);

  static const Json::Value& null_or_object(
      const Json::Value& value,
      const std::string& field);

  static const Json::Value& object_or_string(
      const Json::Value& value,
      const std::string& field);

  static bool has_field(
    const Json::Value& value,
    const std::string& field);

  /**
   * Add (key, value) pairs from the given `right` object into the given `left`
   * object, in place.
   */
  static void update_object(Json::Value& left, const Json::Value& right);

  /* Error on invalid members of a json object. */
  static void check_unexpected_members(
      const Json::Value& value,
      const std::unordered_set<std::string>& valid_members);
};

} // namespace marianatrench

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

#include <boost/filesystem.hpp>
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

  static DexType* dex_type(const Json::Value& value);
  static DexType* dex_type(const Json::Value& value, const std::string& field);

  static DexFieldRef* dex_field(const Json::Value& value);
  static DexFieldRef* dex_field(
      const Json::Value& value,
      const std::string& field);

  static Json::Value parse_json(std::string string);
  static Json::Value parse_json_file(const boost::filesystem::path& path);
  static Json::Value parse_json_file(const std::string& path);

  static std::unique_ptr<Json::StreamWriter> compact_writer();
  static std::unique_ptr<Json::StreamWriter> styled_writer();
  static void write_json_file(
      const boost::filesystem::path& path,
      const Json::Value& value);
  static std::string to_styled_string(const Json::Value& value);

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

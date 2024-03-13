/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <filesystem>
#include <functional>
#include <string>

#include <json/json.h>

namespace marianatrench {

class JsonReader {
 public:
  static Json::Value parse_json(std::string string);
  static Json::Value parse_json_file(const std::filesystem::path& path);
  static Json::Value parse_json_file(const std::string& path);
};

class JsonWriter {
 public:
  static std::unique_ptr<Json::StreamWriter> compact_writer();
  static std::unique_ptr<Json::StreamWriter> styled_writer();
  static void write_json_file(
      const std::filesystem::path& path,
      const Json::Value& value);

  static void write_sharded_json_files(
      const std::filesystem::path& output_directory,
      const std::size_t batch_size,
      const std::size_t total_elements,
      const std::string& filename_prefix,
      const std::function<Json::Value(std::size_t)>& get_json_line);

  static std::string to_styled_string(const Json::Value& value);
};

} // namespace marianatrench

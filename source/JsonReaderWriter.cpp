/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <sstream>

#include <boost/algorithm/string/predicate.hpp>
#include <fmt/format.h>

#include <sparta/WorkQueue.h>

#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

Json::Value JsonReader::parse_json(std::string string) {
  std::istringstream stream(std::move(string));

  static const auto reader = Json::CharReaderBuilder();
  std::string errors;
  Json::Value json;

  if (!Json::parseFromStream(reader, stream, &json, &errors)) {
    throw std::invalid_argument(fmt::format("Invalid json: {}", errors));
  }
  return json;
}

Json::Value JsonReader::parse_json_file(const std::filesystem::path& path) {
  std::ifstream file;
  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    file.open(path, std::ios_base::binary);
  } catch (const std::ifstream::failure&) {
    ERROR(1, "Could not open json file: `{}`.", path.string());
    throw;
  }

  static const auto reader = Json::CharReaderBuilder();
  std::string errors;
  Json::Value json;

  if (!Json::parseFromStream(reader, file, &json, &errors)) {
    throw std::invalid_argument(
        fmt::format("File `{}` is not valid json: {}", path.string(), errors));
  }
  return json;
}

Json::Value JsonReader::parse_json_file(const std::string& path) {
  return parse_json_file(std::filesystem::path(path));
}

namespace {

Json::StreamWriterBuilder compact_writer_builder() {
  Json::StreamWriterBuilder writer;
  writer["indentation"] = "";
  return writer;
}

Json::StreamWriterBuilder styled_writer_builder() {
  Json::StreamWriterBuilder writer;
  writer["indentation"] = "  ";
  return writer;
}

} // namespace

std::unique_ptr<Json::StreamWriter> JsonWriter::compact_writer() {
  static const auto writer_builder = compact_writer_builder();
  return std::unique_ptr<Json::StreamWriter>(writer_builder.newStreamWriter());
}

std::unique_ptr<Json::StreamWriter> JsonWriter::styled_writer() {
  static const auto writer_builder = styled_writer_builder();
  return std::unique_ptr<Json::StreamWriter>(writer_builder.newStreamWriter());
}

void JsonWriter::write_json_file(
    const std::filesystem::path& path,
    const Json::Value& value) {
  std::ofstream file;
  file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
  file.open(path, std::ios_base::binary);
  compact_writer()->write(value, &file);
  file << "\n";
  file.close();
}

void JsonWriter::write_sharded_json_files(
    const std::filesystem::path& output_directory,
    const std::size_t batch_size,
    const std::size_t total_elements,
    const std::string& filename_prefix,
    const std::function<Json::Value(std::size_t)>& get_json_line) {
  // Remove existing files with filename_prefix under output_directory.
  for (auto& file : std::filesystem::directory_iterator(output_directory)) {
    const auto& file_path = file.path();
    if (std::filesystem::is_regular_file(file_path) &&
        boost::starts_with(file_path.filename().string(), filename_prefix)) {
      std::filesystem::remove(file_path);
    }
  }

  const auto total_batch = total_elements / batch_size + 1;
  const auto padded_total_batch = fmt::format("{:0>5}", total_batch);

  auto queue = sparta::work_queue<std::size_t>(
      [&](std::size_t batch) {
        const auto padded_batch = fmt::format("{:0>5}", batch);
        const auto batch_path = output_directory /
            (filename_prefix + padded_batch + "-of-" + padded_total_batch +
             ".json");

        std::ofstream batch_stream;
        batch_stream.open(batch_path, std::ios_base::out);
        if (!batch_stream.is_open()) {
          ERROR(1, "Unable to write json lines to `{}`.", batch_path.native());
          return;
        }

        batch_stream << "// @"
                     << "generated\n";

        // Write the current batch of models to file.
        auto writer = JsonWriter::compact_writer();
        for (std::size_t i = batch_size * batch;
             i < batch_size * (batch + 1) && i < total_elements;
             i++) {
          writer->write(get_json_line(i), &batch_stream);
          batch_stream << "\n";
        }
        batch_stream.close();
      },
      sparta::parallel::default_num_threads());

  for (std::size_t batch = 0; batch < total_batch; batch++) {
    queue.add_item(batch);
  }
  queue.run_all();

  LOG(1, "Wrote json lines to {} shards.", total_batch);
}

std::string JsonWriter::to_styled_string(const Json::Value& value) {
  std::ostringstream string;
  styled_writer()->write(value, &string);
  return string.str();
}

} // namespace marianatrench

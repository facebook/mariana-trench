/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fstream>

#include <mariana-trench/Filesystem.h>
#include <mariana-trench/Log.h>

namespace marianatrench {
namespace filesystem {

void save_string_file(
    const std::filesystem::path& path,
    const std::string_view str) {
  std::ofstream file;
  file.exceptions(std::ios_base::failbit | std::ios_base::badbit);
  file.open(path, std::ios_base::binary);
  file.write(str.data(), str.size());
}

void load_string_file(const std::filesystem::path& path, std::string& str) {
  std::ifstream file;
  file.exceptions(std::ios_base::failbit | std::ios_base::badbit);
  file.open(path, std::ios_base::binary);
  std::size_t size = static_cast<std::size_t>(std::filesystem::file_size(path));
  str.resize(size, '\0');
  file.read(&str[0], size);
}

std::vector<std::string> read_lines(const std::filesystem::path& path) {
  std::ifstream file;
  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    file.open(path, std::ios_base::binary);
  } catch (const std::ifstream::failure&) {
    ERROR(1, "Could not open file: `{}`.", path);
    throw;
  }

  std::vector<std::string> lines;
  std::string line;
  try {
    while (std::getline(file, line)) {
      lines.push_back(line);
    }
  } catch (const std::ifstream::failure&) {
    if (!file.eof()) {
      ERROR(1, "Error reading file: `{}`.", path);
      throw;
    }
  }

  return lines;
}

} // namespace filesystem
} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fstream>

#include <mariana-trench/Filesystem.h>

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

} // namespace filesystem
} // namespace marianatrench

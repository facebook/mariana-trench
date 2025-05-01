/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace marianatrench {
namespace filesystem {

/* Write given str to file at path */
void save_string_file(
    const std::filesystem::path& path,
    const std::string_view str);

/* Load contents of file at path to str */
void load_string_file(const std::filesystem::path& path, std::string& str);

std::vector<std::string> read_lines(const std::filesystem::path& path);

} // namespace filesystem
} // namespace marianatrench

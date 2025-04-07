/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/FilesCoverage.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Registry.h>
#include <mariana-trench/Statistics.h>
#include <mariana-trench/Timer.h>

namespace marianatrench {

FilesCoverage FilesCoverage::compute(const Registry& registry) {
  auto registry_covered_files = registry.compute_files();

  // TODO(T215715156): Check APK for interface-only files and consider them
  // covered.

  return FilesCoverage(std::move(registry_covered_files));
}

void FilesCoverage::dump(const std::filesystem::path& output_path) const {
  std::ofstream output_file;
  output_file.open(output_path, std::ios_base::out);
  if (!output_file.is_open()) {
    ERROR(
        1, "Unable to write file coverage info to `{}`.", output_path.native());
    return;
  }

  for (const auto& path : files_) {
    output_file << path << std::endl;
  }

  output_file.close();
}

} // namespace marianatrench

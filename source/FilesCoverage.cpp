/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fstream>

#include <Walkers.h>

#include <mariana-trench/EventLogger.h>
#include <mariana-trench/FilesCoverage.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/Registry.h>
#include <mariana-trench/Statistics.h>
#include <mariana-trench/Timer.h>

namespace marianatrench {

namespace {

std::unordered_set<std::string> compute_files(
    const Registry& registry,
    const Positions& positions) {
  std::unordered_set<std::string> covered_paths;
  for (const auto& [method, model] : UnorderedIterable(registry.models())) {
    if (method->get_code() == nullptr || model.skip_analysis()) {
      continue;
    }

    const auto* path = positions.get_path(method->dex_method());
    if (path) {
      covered_paths.insert(*path);
    }
  }
  return covered_paths;
}

} // namespace

FilesCoverage FilesCoverage::compute(
    const Registry& registry,
    const Positions& positions,
    const DexStoresVector& stores) {
  auto registry_covered_files = compute_files(registry, positions);

  // Check APK for implementationless files and consider them covered.
  InsertOnlyConcurrentSet<const std::string*> file_has_implementation;
  for (const auto& scope : DexStoreClassesIterator(stores)) {
    walk::parallel::methods(
        scope,
        [&positions, &file_has_implementation](const DexMethod* dex_method) {
          const auto* source_file = positions.get_path(dex_method);
          if (source_file == nullptr) {
            return;
          }

          if (!is_abstract(dex_method)) {
            file_has_implementation.insert(source_file);
          }
        });
  }

  auto all_files_containing_methods = positions.all_paths();
  for (const auto* path : all_files_containing_methods) {
    if (file_has_implementation.find(path) == file_has_implementation.end()) {
      registry_covered_files.insert(*path);
    }
  }

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

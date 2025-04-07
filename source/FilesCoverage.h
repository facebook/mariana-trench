/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_set>

#include <json/json.h>

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Registry.h>

namespace marianatrench {

class FilesCoverage final {
 private:
  explicit FilesCoverage(std::unordered_set<std::string> files)
      : files_(std::move(files)) {}

 public:
  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(FilesCoverage)

  static FilesCoverage compute(
      const Registry& registry,
      const Positions& positions,
      const DexStoresVector& stores);

  void dump(const std::filesystem::path& output_path) const;

 private:
  std::unordered_set<std::string> files_;
};

} // namespace marianatrench

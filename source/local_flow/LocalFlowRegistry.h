/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <filesystem>
#include <vector>

#include <ConcurrentContainers.h>

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/local_flow/LocalFlowResult.h>

namespace marianatrench {
namespace local_flow {

/**
 * Holds all local flow analysis results (method and class).
 * Thread-safe for concurrent method result insertion via ConcurrentMap.
 */
class LocalFlowRegistry {
 public:
  LocalFlowRegistry() = default;
  MOVE_CONSTRUCTOR_ONLY(LocalFlowRegistry)

  /* Thread-safe. */
  void set(const Method* method, LocalFlowMethodResult result);

  void set_class_results(std::vector<LocalFlowClassResult> results);

  const InsertOnlyConcurrentMap<const Method*, LocalFlowMethodResult>&
  method_results() const;
  const std::vector<LocalFlowClassResult>& class_results() const;

  /**
   * Write results as sharded JSON files to the output directory.
   * Uses JsonWriter::write_sharded_json_files with prefix "local_flow@".
   */
  void to_sharded_json(
      const std::filesystem::path& output_directory,
      std::size_t batch_size = JsonValidation::k_default_shard_limit) const;

 private:
  InsertOnlyConcurrentMap<const Method*, LocalFlowMethodResult> method_results_;
  std::vector<LocalFlowClassResult> class_results_;
};

} // namespace local_flow
} // namespace marianatrench

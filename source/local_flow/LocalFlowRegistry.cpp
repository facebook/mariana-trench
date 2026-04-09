/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/local_flow/LocalFlowRegistry.h>

#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/Log.h>

namespace marianatrench {
namespace local_flow {

void LocalFlowRegistry::set(
    const Method* method,
    LocalFlowMethodResult result) {
  method_results_.emplace(method, std::move(result));
}

void LocalFlowRegistry::set_class_results(
    std::vector<LocalFlowClassResult> results) {
  class_results_ = std::move(results);
}

const InsertOnlyConcurrentMap<const Method*, LocalFlowMethodResult>&
LocalFlowRegistry::method_results() const {
  return method_results_;
}

const std::vector<LocalFlowClassResult>& LocalFlowRegistry::class_results()
    const {
  return class_results_;
}

void LocalFlowRegistry::to_sharded_json(
    const std::filesystem::path& output_directory,
    std::size_t batch_size) const {
  // Collect non-empty method results into a vector for indexed access
  std::vector<const LocalFlowMethodResult*> method_ptrs;
  for (const auto& [_, result] : UnorderedIterable(method_results_)) {
    if (!result.constraints.empty()) {
      method_ptrs.push_back(&result);
    }
  }

  std::size_t total = method_ptrs.size() + class_results_.size();

  auto get_json_line = [&](std::size_t i) -> Json::Value {
    if (i < method_ptrs.size()) {
      return method_ptrs[i]->to_json();
    }
    return class_results_[i - method_ptrs.size()].to_json();
  };

  JsonWriter::write_sharded_json_files(
      output_directory, batch_size, total, "local_flow@", get_json_line);

  LOG(1,
      "Wrote {} method results and {} class results as sharded local flow output.",
      method_ptrs.size(),
      class_results_.size());
}

} // namespace local_flow
} // namespace marianatrench

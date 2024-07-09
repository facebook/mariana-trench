/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <unordered_set>

#include <json/json.h>

#include <ConcurrentContainers.h>

#include <mariana-trench/CallGraph.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/Registry.h>

namespace marianatrench {

class Dependencies final {
 public:
  explicit Dependencies(
      const Options& options,
      const Heuristics& heuristics,
      const Methods& methods,
      const Overrides& overrides,
      const CallGraph& call_graph,
      const Registry& registry);

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Dependencies)

  /**
   * Return the set of dependencies for the given method, i.e the set of
   * possible callers.
   */
  const std::unordered_set<const Method*>& dependencies(
      const Method* method) const;

  Json::Value to_json(const Method* method) const;
  Json::Value to_json() const;

  void dump_dependencies(
      const std::filesystem::path& output_directory,
      const std::size_t batch_size =
          JsonValidation::k_default_shard_limit) const;

 private:
  ConcurrentMap<const Method*, std::unordered_set<const Method*>> dependencies_;
  std::unordered_set<const Method*> empty_method_set_;
};

} // namespace marianatrench

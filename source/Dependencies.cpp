/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <vector>

#include <fmt/format.h>

#include <json/value.h>
#include <sparta/WorkQueue.h>

#include <IRInstruction.h>
#include <Show.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Dependencies.h>
#include <mariana-trench/Heuristics.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>

namespace marianatrench {

Dependencies::Dependencies(
    const Options& options,
    const Methods& methods,
    const Overrides& overrides,
    const CallGraph& call_graph,
    const Registry& registry) {
  ConcurrentSet<const Method*> warn_many_overrides;

  auto queue = sparta::work_queue<const Method*>(
      [&](const Method* caller) {
        auto add_caller_as_dependency =
            [&](const Method* /* method */,
                std::unordered_set<const Method*>& dependencies,
                bool /* is_new */) {
              auto* code = caller->get_code();
              if (!code) {
                return;
              }

              auto model = registry.get(caller);
              if (model.skip_analysis()) {
                return;
              }

              dependencies.insert(caller);
            };

        auto add_dependency = [&](const CallTarget& call_target) {
          if (!call_target.resolved()) {
            return;
          }

          dependencies_.update(
              call_target.resolved_base_callee(), add_caller_as_dependency);

          if (!call_target.is_virtual()) {
            // We don't add a dependency for overrides of direct invocations.
            return;
          }

          auto model = registry.get(call_target.resolved_base_callee());

          if (model.no_join_virtual_overrides()) {
            return;
          }

          if (Heuristics::kWarnOverrideThreshold &&
              overrides.get(call_target.resolved_base_callee()).size() >=
                  *Heuristics::kWarnOverrideThreshold) {
            warn_many_overrides.insert(call_target.resolved_base_callee());
          }

          for (const auto* override : call_target.overrides()) {
            dependencies_.update(override, add_caller_as_dependency);
          }
        };

        auto callees = call_graph.callees(caller);

        for (const auto& call_target : callees) {
          add_dependency(call_target);
        }

        const auto& artificial_callees = call_graph.artificial_callees(caller);

        for (const auto& [instruction, callees] : artificial_callees) {
          for (const auto& artificial_callee : callees) {
            add_dependency(artificial_callee.call_target);
          }
        }
      },
      sparta::parallel::default_num_threads());
  for (const auto* method : methods) {
    queue.add_item(method);
  }
  queue.run_all();

  for (const auto* method : warn_many_overrides) {
    WARNING(
        1,
        "Method `{}` has {} overrides, consider marking it with `no-join-virtual-overrides` if the analysis is slow.",
        method->show(),
        overrides.get(method).size());
  }

  if (options.dump_dependencies()) {
    dump_dependencies(options.dependencies_output_path());
  }
}

const std::unordered_set<const Method*>& Dependencies::dependencies(
    const Method* method) const {
  // Note that `find` is not thread-safe, but this is fine because
  // `dependencies_` is read-only after the constructor completed.
  auto found = dependencies_.find(method);
  if (found != dependencies_.end()) {
    return found->second;
  } else {
    return empty_method_set_;
  }
}

Json::Value Dependencies::to_json(const Method* method) const {
  auto dependencies = dependencies_.find(method);
  mt_assert(dependencies != dependencies_.end());

  auto dependencies_value = Json::Value(Json::arrayValue);
  for (const auto* dependency : dependencies->second) {
    dependencies_value.append(Json::Value(dependency->show()));
  }

  JsonValidation::nonempty_array(dependencies_value);

  return dependencies_value;
}

Json::Value Dependencies::to_json() const {
  auto value = Json::Value(Json::objectValue);
  for (const auto& [method, _dependencies] : dependencies_) {
    value[method->show()] = to_json(method);
  }
  return value;
}

void Dependencies::dump_dependencies(
    const std::filesystem::path& output_directory,
    const std::size_t batch_size) const {
  LOG(1, "Writing dependencies to `{}`", output_directory.native());

  std::vector<const Method*> methods;
  methods.reserve(dependencies_.size());
  for (const auto& [method, _dependencies] : dependencies_) {
    methods.push_back(method);
  }

  std::size_t total_elements = methods.size();
  auto get_json_line = [&](std::size_t i) -> Json::Value {
    auto value = Json::Value(Json::objectValue);
    const auto* method = methods.at(i);
    value[method->show()] = to_json(method);
    return value;
  };

  JsonValidation::write_sharded_json_files(
      output_directory,
      batch_size,
      total_elements,
      "dependencies@",
      get_json_line);
}

} // namespace marianatrench

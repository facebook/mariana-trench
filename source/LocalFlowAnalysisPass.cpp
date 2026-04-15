/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fstream>

#include <json/json.h>

#include <mariana-trench/ClassIntervals.h>
#include <mariana-trench/ControlFlowGraphs.h>
#include <mariana-trench/LocalFlowAnalysisPass.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/OperatingSystem.h>
#include <mariana-trench/Statistics.h>
#include <mariana-trench/Timer.h>
#include <mariana-trench/local_flow/LocalFlowAnalysis.h>
#include <mariana-trench/local_flow/LocalFlowRegistry.h>

namespace marianatrench {

void LocalFlowAnalysisPass::run(Context& context) {
  // Build CFGs if not already built by a prior pass (e.g., TaintAnalysisPass).
  // Note: Positions must be created before CFGs because build_cfg() destroys
  // the raw linear instruction list needed for position/line indexing.
  if (context.control_flow_graphs == nullptr) {
    Timer cfg_timer;
    LOG(1, "Building control flow graphs...");
    context.control_flow_graphs =
        std::make_unique<ControlFlowGraphs>(context.stores);
    context.statistics->log_time("control_flow_graphs", cfg_timer);
    LOG(1,
        "Built control flow graphs in {:.2f}s. Memory used, RSS: {:.2f}GB",
        cfg_timer.duration_in_seconds(),
        resident_set_size_in_gb());
  }

  // Build class_intervals if not already built by a prior pass.
  if (context.class_intervals == nullptr) {
    Timer class_intervals_timer;
    LOG(1, "Computing class intervals...");
    context.class_intervals = std::make_unique<ClassIntervals>(
        *context.options, context.options->analysis_mode(), context.stores);
    context.statistics->log_time("class_intervals", class_intervals_timer);
    LOG(1,
        "Computed class intervals in {:.2f}s.",
        class_intervals_timer.duration_in_seconds());
  }

  Timer local_flow_timer;
  local_flow::LocalFlowRegistry registry;
  local_flow::LocalFlowAnalysis::run(
      context,
      registry,
      context.options->max_local_flow_structure_depth(),
      context.options->sequential());
  context.statistics->log_time("local_flow", local_flow_timer);
  LOG(1,
      "Local flow analysis complete in {:.2f}s.",
      local_flow_timer.duration_in_seconds());

  Timer output_timer;
  auto output_dir = context.options->local_flow_output_path();
  LOG(1, "Writing sharded local flow results to `{}`.", output_dir.native());
  registry.to_sharded_json(output_dir);

  // Write metadata for SAPP AnalysisOutput discovery
  auto metadata_path = output_dir / "local_flow-metadata.json";
  Json::Value metadata;
  metadata["tool"] = "mariana_trench";
  metadata["filename_spec"] = "local_flow@*.json";
  const auto& commit_hash = context.options->commit_hash();
  metadata["commit"] = commit_hash.has_value() ? commit_hash.value() : "";
  std::ofstream metadata_file(metadata_path);
  metadata_file << metadata.toStyledString();

  context.statistics->log_time("local_flow_output", output_timer);
  LOG(1,
      "Wrote local flow results in {:.2f}s.",
      output_timer.duration_in_seconds());
}

} // namespace marianatrench

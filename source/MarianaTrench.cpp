/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/program_options.hpp>

#include <RedexContext.h>

#include <mariana-trench/AnalysisPass.h>
#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/EventLogger.h>
#include <mariana-trench/Fields.h>
#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/MarianaTrench.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/OperatingSystem.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/Statistics.h>
#include <mariana-trench/Timer.h>

namespace marianatrench {

MarianaTrench::MarianaTrench()
    : Tool(
          "Mariana Trench",
          "Taint Analysis for Android",
          /* verbose */ false) {}

namespace program_options = boost::program_options;

void MarianaTrench::initialize_context(Context& context) {
  context.artificial_methods = std::make_unique<ArtificialMethods>(
      *context.kind_factory, context.stores);

  Timer methods_timer;
  LOG(1, "Storing methods...");
  context.methods = std::make_unique<Methods>(context.stores);
  if (context.options->dump_methods()) {
    auto method_list = Json::Value(Json::arrayValue);
    for (const auto* method : *context.methods) {
      method_list.append(method->signature());
    }
    auto methods_path = context.options->methods_output_path();
    LOG(1, "Writing methods to `{}`.", methods_path.native());
    JsonWriter::write_json_file(methods_path, method_list);
  }
  LOG(1,
      "Stored all methods in {:.2f}s. Memory used, RSS: {:.2f}GB",
      methods_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  Timer fields_timer;
  LOG(1, "Storing fields...");
  context.fields = std::make_unique<Fields>(context.stores);
  LOG(1,
      "Stored all fields in {:.2f}s. Memory used, RSS: {:.2f}GB",
      fields_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  Timer index_timer;
  LOG(1, "Building source index...");
  context.positions =
      std::make_unique<Positions>(*context.options, context.stores);
  context.statistics->log_time("source_index", index_timer);
  LOG(1,
      "Built source index in {:.2f}s. Memory used, RSS: {:.2f}GB",
      index_timer.duration_in_seconds(),
      resident_set_size_in_gb());
}

namespace {

std::vector<std::string> filter_existing_jars(
    const std::vector<std::string>& system_jar_paths) {
  std::vector<std::string> results;

  for (const auto& system_jar_path : system_jar_paths) {
    if (std::filesystem::exists(system_jar_path)) {
      results.push_back(system_jar_path);
    } else {
      ERROR(1, "Unable to find system jar `{}`", system_jar_path);
    }
  }

  return results;
}

} // namespace

void MarianaTrench::run(const program_options::variables_map& variables) {
  Context context;
  std::filesystem::path json_file_path =
      std::filesystem::path(variables["config"].as<std::string>());

  context.options = Options::from_json_file(json_file_path);
  const auto& options = *context.options;

  if (auto graphql_metadata_path = options.graphql_metadata_path()) {
    LOG(1, "GraphQL metadata path: `{}`", *graphql_metadata_path);
  }

  if (auto heuristics_path = options.heuristics_path()) {
    Heuristics::init_from_file(*heuristics_path);
  }

  EventLogger::init_event_logger(context.options.get());

  auto system_jar_paths = filter_existing_jars(options.system_jar_paths());

  Timer initialization_timer;
  LOG(1, "Initializing Redex...");
  context.stores = init(
      boost::join(system_jar_paths, ","),
      options.apk_directory(),
      options.dex_directory(),
      /* balloon */ true,
      /* throw_on_balloon_error */ true,
      /* support_dex_version */ 39);

  redex::process_proguard_configurations(options, context.stores);
  if (context.options->remove_unreachable_code()) {
    redex::remove_unreachable(options, context.stores);
  }

  DexStore external_store("external classes");
  external_store.add_classes(g_redex->external_classes());
  context.stores.push_back(external_store);

  context.statistics->log_time("redex_init", initialization_timer);
  LOG(1,
      "Redex initialized in {:.2f}s.",
      initialization_timer.duration_in_seconds());

  initialize_context(context);

  for (auto kind : options.analysis_passes()) {
    auto pass = AnalysisPass::create(kind);
    LOG(1, "Running pass: {}...", pass->name());
    pass->run(context);
  }
}

} // namespace marianatrench

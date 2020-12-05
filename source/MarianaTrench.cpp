/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>
#include <stdexcept>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/program_options.hpp>

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/ClassProperties.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Dependencies.h>
#include <mariana-trench/Interprocedural.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/MarianaTrench.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/Scheduler.h>
#include <mariana-trench/Statistics.h>
#include <mariana-trench/Timer.h>
#include <mariana-trench/Types.h>
#include <mariana-trench/model-generator/ModelGenerator.h>

namespace marianatrench {

MarianaTrench::MarianaTrench()
    : Tool(
          "Mariana Trench",
          "Taint Analysis for Android",
          /* verbose */ false) {}

namespace program_options = boost::program_options;

void MarianaTrench::add_options(
    program_options::options_description& options) const {
  Options::add_options(options);
}

Registry MarianaTrench::analyze(Context& context) {
  context.artificial_methods =
      std::make_unique<ArtificialMethods>(*context.kinds, context.stores);
  context.methods = std::make_unique<Methods>(context.stores);

  Timer index_timer;
  LOG(1, "Building source index...");
  context.positions =
      std::make_unique<Positions>(*context.options, context.stores);
  context.statistics->log_time("source_index", index_timer);
  LOG(1, "Built source index in {:.2f}s.", index_timer.duration_in_seconds());

  Timer types_timer;
  LOG(1, "Inferring types...");
  context.types = std::make_unique<Types>(context.stores);
  context.statistics->log_time("types", types_timer);
  LOG(1, "Inferred types in {:.2f}s.", types_timer.duration_in_seconds());

  Timer class_properties_timer;
  context.class_properties = std::make_unique<ClassProperties>(
      *context.options, context.stores, *context.features);
  context.statistics->log_time("class_properties", class_properties_timer);
  LOG(1,
      "Created class properties in {:.2f}s.",
      class_properties_timer.duration_in_seconds());

  Timer class_hierarchies_timer;
  LOG(1, "Building class hierarchies...");
  context.class_hierarchies =
      std::make_unique<ClassHierarchies>(context.stores);
  context.statistics->log_time("class_hierarchies", class_hierarchies_timer);
  LOG(1,
      "Built class hierarchies in {:.2f}s.",
      class_hierarchies_timer.duration_in_seconds());

  Timer overrides_timer;
  LOG(1, "Building override graph...");
  context.overrides =
      std::make_unique<Overrides>(*context.methods, context.stores);
  context.statistics->log_time("overrides", overrides_timer);
  LOG(1,
      "Built override graph in {:.2f}s.",
      overrides_timer.duration_in_seconds());

  Timer call_graph_timer;
  LOG(1, "Building call graph...");
  context.call_graph = std::make_unique<CallGraph>(
      *context.options,
      *context.methods,
      *context.types,
      *context.class_hierarchies,
      *context.overrides,
      *context.features);
  context.statistics->log_time("call_graph", call_graph_timer);
  LOG(1,
      "Built call graph in {:.2f}s.",
      call_graph_timer.duration_in_seconds());

  std::vector<Model> generated_models;
  if (!context.options->skip_model_generation()) {
    Timer generation_timer;
    LOG(1, "Generating models...");
    generated_models = ModelGenerator::run(context);
    context.statistics->log_time("models_generation", generation_timer);
    LOG(1,
        "Generated {} models in {:.2f}s.",
        generated_models.size(),
        generation_timer.duration_in_seconds());
  } else {
    LOG(1, "Skipped model generation.");
  }

  // Add models for artificial methods.
  {
    auto models = context.artificial_methods->models(context);
    generated_models.insert(
        generated_models.end(), models.begin(), models.end());
  }

  Timer registry_timer;
  LOG(1, "Initializing models...");
  auto registry = Registry::load(
      context, *context.options, context.stores, generated_models);
  context.statistics->log_time("registry_init", registry_timer);
  LOG(1,
      "Initialized {} models in {:.2f}s.",
      registry.models_size(),
      registry_timer.duration_in_seconds());

  Timer rules_timer;
  LOG(1, "Initializing rules...");
  context.rules =
      std::make_unique<Rules>(Rules::load(context, *context.options));
  context.statistics->log_time("rules_init", rules_timer);
  LOG(1,
      "Initialized {} rules in {:.2f}s.",
      context.rules->size(),
      rules_timer.duration_in_seconds());

  Timer dependencies_timer;
  LOG(1, "Building dependency graph...");
  context.dependencies = std::make_unique<Dependencies>(
      *context.options,
      *context.methods,
      *context.overrides,
      *context.call_graph,
      registry);
  context.statistics->log_time("dependencies", dependencies_timer);
  LOG(1,
      "Built dependency graph in {:.2f}s.",
      dependencies_timer.duration_in_seconds());

  Timer scheduler_timer;
  LOG(1, "Building the analysis schedule...");
  context.scheduler =
      std::make_unique<Scheduler>(*context.methods, *context.dependencies);
  context.statistics->log_time("scheduler", scheduler_timer);
  LOG(1,
      "Built the analysis schedule in {:.2f}s.",
      scheduler_timer.duration_in_seconds());

  Timer analysis_timer;
  LOG(1, "Analyzing...");
  Interprocedural::run_analysis(context, registry);
  context.statistics->log_time("fixpoint", analysis_timer);
  LOG(1,
      "Analyzed {} models in {:.2f}s. Found {} issues!",
      registry.models_size(),
      analysis_timer.duration_in_seconds(),
      registry.issues_size());

  Timer remove_collapsed_traces_timer;
  LOG(2, "Removing invalid traces due to collapsing...");
  registry.postprocess_remove_collapsed_traces();
  context.statistics->log_time(
      "remove_collapsed_traces", remove_collapsed_traces_timer);
  LOG(2,
      "Removed invalid traces in {:.2f}s.",
      remove_collapsed_traces_timer.duration_in_seconds());

  return registry;
}

namespace {

std::vector<std::string> filter_existing_jars(
    const std::vector<std::string>& system_jar_paths) {
  std::vector<std::string> results;

  for (const auto& system_jar_path : system_jar_paths) {
    if (boost::filesystem::exists(system_jar_path)) {
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

  context.options = std::make_unique<Options>(variables);
  const auto& options = *context.options;

  auto system_jar_paths = filter_existing_jars(options.system_jar_paths());

  Timer initialization_timer;
  LOG(1, "Initializing Redex...");
  context.stores = init(
      boost::join(system_jar_paths, ","),
      options.apk_directory(),
      options.dex_directory(),
      /* balloon */ true,
      /* support_dex_version */ 37);

  redex::remove_unreachable(
      options.proguard_configuration_paths(),
      context.stores,
      options.removed_symbols_output_path());

  DexStore external_store("external classes");
  external_store.add_classes(g_redex->external_classes());
  context.stores.push_back(external_store);

  context.statistics->log_time("redex_init", initialization_timer);
  LOG(1,
      "Redex initialized in {:.2f}s.",
      initialization_timer.duration_in_seconds());

  auto registry = analyze(context);

  Timer output_timer;
  auto models_path = options.models_output_path();
  LOG(1, "Writing models to `{}`.", models_path.native());
  registry.dump_models(models_path);
  context.statistics->log_time("dump_models", output_timer);
  LOG(1, "Wrote models in {:.2f}s.", output_timer.duration_in_seconds());

  auto metadata_path = options.metadata_output_path();
  LOG(1, "Writing metadata to `{}`.", metadata_path.native());
  registry.dump_metadata(/* path */ metadata_path);
}

} // namespace marianatrench

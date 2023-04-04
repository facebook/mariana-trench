/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>
#include <stdexcept>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/program_options.hpp>

#include <RedexContext.h>

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/ClassProperties.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/ControlFlowGraphs.h>
#include <mariana-trench/Dependencies.h>
#include <mariana-trench/EventLogger.h>
#include <mariana-trench/FieldCache.h>
#include <mariana-trench/Fields.h>
#include <mariana-trench/Highlights.h>
#include <mariana-trench/Interprocedural.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/LifecycleMethods.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/MarianaTrench.h>
#include <mariana-trench/MethodMappings.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/ModelGeneration.h>
#include <mariana-trench/OperatingSystem.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/PostprocessTraces.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/Scheduler.h>
#include <mariana-trench/Statistics.h>
#include <mariana-trench/Timer.h>
#include <mariana-trench/Types.h>
#include <mariana-trench/UsedKinds.h>
#include <mariana-trench/shim-generator/ShimGeneration.h>
#include <mariana-trench/shim-generator/Shims.h>

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
    JsonValidation::write_json_file(methods_path, method_list);
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

  Timer control_flow_graphs_timer;
  LOG(1, "Building control flow graphs...");
  context.control_flow_graphs =
      std::make_unique<ControlFlowGraphs>(context.stores);
  context.statistics->log_time(
      "control_flow_graphs", control_flow_graphs_timer);
  LOG(1,
      "Built control flow graphs in {:.2f}s. Memory used, RSS: {:.2f}GB",
      control_flow_graphs_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  Timer types_timer;
  LOG(1, "Inferring types...");
  context.types = std::make_unique<Types>(*context.options, context.stores);
  context.statistics->log_time("types", types_timer);
  LOG(1,
      "Inferred types in {:.2f}s. Memory used, RSS: {:.2f}GB",
      types_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  Timer class_hierarchies_timer;
  LOG(1, "Building class hierarchies...");
  context.class_hierarchies =
      std::make_unique<ClassHierarchies>(*context.options, context.stores);
  context.statistics->log_time("class_hierarchies", class_hierarchies_timer);
  LOG(1,
      "Built class hierarchies in {:.2f}s. Memory used, RSS: {:.2f}GB",
      class_hierarchies_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  Timer field_cache_timer;
  LOG(1, "Building fields cache...");
  context.field_cache =
      std::make_unique<FieldCache>(*context.class_hierarchies, context.stores);
  context.statistics->log_time("fields", field_cache_timer);
  LOG(1,
      "Built fields cache in {:.2f}s. Memory used, RSS: {:.2f}GB",
      field_cache_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  Timer lifecycle_methods_timer;
  LOG(1, "Creating life-cycle wrapper methods...");
  LifecycleMethods::run(
      *context.options, *context.class_hierarchies, *context.methods);
  context.statistics->log_time("lifecycle_methods", lifecycle_methods_timer);
  LOG(1,
      "Created lifecycle methods in {:.2f}s. Memory used, RSS: {:.2f}GB",
      lifecycle_methods_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  Timer overrides_timer;
  LOG(1, "Building override graph...");
  context.overrides = std::make_unique<Overrides>(
      *context.options, *context.methods, context.stores);
  context.statistics->log_time("overrides", overrides_timer);
  LOG(1,
      "Built override graph in {:.2f}s. Memory used, RSS: {:.2f}GB",
      overrides_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  std::vector<Model> generated_models;
  std::vector<FieldModel> generated_field_models;

  // Scope for MethodMappings and MethodToShimMap as they are only used for shim
  // and model generation steps.
  {
    Timer method_mapping_timer;
    LOG(1,
        "Building method mappings for shim/model generation over {} methods",
        context.methods->size());
    MethodMappings method_mappings{*context.methods};

    LOG(1,
        "Generated method mappings in {:.2f}s. Memory used, RSS: {:.2f}GB",
        method_mapping_timer.duration_in_seconds(),
        resident_set_size_in_gb());

    Timer shims_timer;
    LOG(1, "Creating Shims...");
    Shims shims = ShimGeneration::run(context, method_mappings);
    LOG(1,
        "Created Shims in {:.2f}s. Memory used, RSS: {:.2f}GB",
        shims_timer.duration_in_seconds(),
        resident_set_size_in_gb());

    Timer call_graph_timer;
    LOG(1, "Building call graph...");
    context.call_graph = std::make_unique<CallGraph>(
        *context.options,
        *context.methods,
        *context.fields,
        *context.types,
        *context.class_hierarchies,
        *context.overrides,
        *context.feature_factory,
        shims,
        method_mappings);
    context.statistics->log_time("call_graph", call_graph_timer);
    LOG(1,
        "Built call graph in {:.2f}s. Memory used, RSS: {:.2f}GB",
        call_graph_timer.duration_in_seconds(),
        resident_set_size_in_gb());

    Timer generation_timer;
    LOG(1, "Generating models...");
    auto model_generator_result =
        ModelGeneration::run(context, method_mappings);
    generated_models = model_generator_result.method_models;
    generated_field_models = model_generator_result.field_models;
    context.statistics->log_time("models_generation", generation_timer);
    LOG(1,
        "Generated {} models and {} field models in {:.2f}s. Memory used, RSS: {:.2f}GB",
        generated_models.size(),
        generated_field_models.size(),
        generation_timer.duration_in_seconds(),
        resident_set_size_in_gb());
  } // end MethodMappings and MethodToShimMap

  LOG(1,
      "Reset MethodToShims and Method Mappings. Memory used, RSS: {:.2f}GB",
      resident_set_size_in_gb());

  // Add models for artificial methods.
  {
    auto models = context.artificial_methods->models(context);
    generated_models.insert(
        generated_models.end(), models.begin(), models.end());
  }

  Timer registry_timer;
  LOG(1, "Initializing models...");
  auto registry = Registry::load(
      context, *context.options, generated_models, generated_field_models);
  context.statistics->log_time("registry_init", registry_timer);
  LOG(1,
      "Initialized {} models and {} field models in {:.2f}s. Memory used, RSS: {:.2f}GB",
      registry.models_size(),
      registry.field_models_size(),
      registry_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  Timer rules_timer;
  LOG(1, "Initializing rules...");
  context.rules =
      std::make_unique<Rules>(Rules::load(context, *context.options));
  context.statistics->log_time("rules_init", rules_timer);
  LOG(1,
      "Initialized {} rules in {:.2f}s. Memory used, RSS: {:.2f}GB",
      context.rules->size(),
      rules_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  Timer transforms_timer;
  LOG(1, "Initializing used transform kinds...");
  context.used_kinds = std::make_unique<UsedKinds>(
      UsedKinds::from_rules(*context.rules, *context.transforms));
  LOG(1,
      "Initialized {} source/sink transform kinds and {} propagation transform kinds in {:.2f}s."
      "Memory used, RSS: {:.2f}GB",
      context.used_kinds->source_sink_size(),
      context.used_kinds->propagation_size(),
      transforms_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  Timer kind_pruning_timer;
  LOG(1, "Removing unused Kinds...");
  int num_removed = UsedKinds::remove_unused_kinds(
                        *context.rules,
                        *context.kind_factory,
                        *context.methods,
                        *context.artificial_methods,
                        registry)
                        .size();
  context.statistics->log_time("prune_kinds", rules_timer);
  LOG(1,
      "Removed {} kinds in {:.2f}s.",
      num_removed,
      kind_pruning_timer.duration_in_seconds());

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
      "Built dependency graph in {:.2f}s. Memory used, RSS: {:.2f}GB",
      dependencies_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  if (!context.options->skip_analysis()) {
    Timer class_properties_timer;
    context.class_properties = std::make_unique<ClassProperties>(
        *context.options,
        context.stores,
        *context.feature_factory,
        *context.dependencies);
    context.statistics->log_time("class_properties", class_properties_timer);
    LOG(1,
        "Created class properties in {:.2f}s. Memory used, RSS: {:.2f}GB",
        class_properties_timer.duration_in_seconds(),
        resident_set_size_in_gb());

    Timer scheduler_timer;
    LOG(1, "Building the analysis schedule...");
    context.scheduler =
        std::make_unique<Scheduler>(*context.methods, *context.dependencies);
    context.statistics->log_time("scheduler", scheduler_timer);
    LOG(1,
        "Built the analysis schedule in {:.2f}s. Memory used, RSS: {:.2f}GB",
        scheduler_timer.duration_in_seconds(),
        resident_set_size_in_gb());

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
    PostprocessTraces::remove_collapsed_traces(registry, context);
    context.statistics->log_time(
        "remove_collapsed_traces", remove_collapsed_traces_timer);
    LOG(2,
        "Removed invalid traces in {:.2f}s.",
        remove_collapsed_traces_timer.duration_in_seconds());
  } else {
    LOG(2, "Skipped taint analysis.");
  }

  if (!context.options->skip_source_indexing()) {
    Timer augment_positions_timer;
    LOG(1, "Augmenting positions...");
    Highlights::augment_positions(registry, context);
    context.statistics->log_time("augment_positions", augment_positions_timer);
    LOG(1,
        "Augmented positions in {:.2f}s.",
        augment_positions_timer.duration_in_seconds());
  } else {
    LOG(2, "Skipped augmenting positions.");
  }

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

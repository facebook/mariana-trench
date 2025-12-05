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

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/ClassIntervals.h>
#include <mariana-trench/ClassProperties.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/ControlFlowGraphs.h>
#include <mariana-trench/Dependencies.h>
#include <mariana-trench/EventLogger.h>
#include <mariana-trench/FieldCache.h>
#include <mariana-trench/Fields.h>
#include <mariana-trench/FilesCoverage.h>
#include <mariana-trench/Highlights.h>
#include <mariana-trench/Interprocedural.h>
#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/LifecycleMethods.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/MarianaTrench.h>
#include <mariana-trench/MethodMappings.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/OperatingSystem.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Overrides.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/PostprocessTraces.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/RulesCoverage.h>
#include <mariana-trench/Scheduler.h>
#include <mariana-trench/Statistics.h>
#include <mariana-trench/Timer.h>
#include <mariana-trench/Types.h>
#include <mariana-trench/UsedKinds.h>
#include <mariana-trench/shim-generator/ShimGeneration.h>
#include <mariana-trench/shim-generator/Shims.h>
#include <mariana-trench/ListingCommands.h>
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {

MarianaTrench::MarianaTrench()
    : Tool(
          "Mariana Trench",
          "Taint Analysis for Android",
          /* verbose */ false) {}

namespace program_options = boost::program_options;

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
  context.class_hierarchies = std::make_unique<ClassHierarchies>(
      *context.options, context.options->analysis_mode(), context.stores);
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

  Timer overrides_timer;
  LOG(1, "Building override graph...");
  context.overrides = std::make_unique<Overrides>(
      *context.options,
      context.options->analysis_mode(),
      *context.methods,
      context.stores);
  context.statistics->log_time("overrides", overrides_timer);
  LOG(1,
      "Built override graph in {:.2f}s. Memory used, RSS: {:.2f}GB",
      overrides_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  Timer class_intervals_timer;
  LOG(1, "Computing class intervals...");
  context.class_intervals = std::make_unique<ClassIntervals>(
      *context.options, context.options->analysis_mode(), context.stores);
  context.statistics->log_time("class_intervals", class_intervals_timer);
  LOG(1,
      "Computed class intervals in {:.2f}s. Memory used, RSS: {:.2f}GB",
      class_intervals_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  Timer lifecycle_methods_timer;
  LOG(1, "Creating life-cycle wrapper methods...");
  auto lifecycle_methods = LifecycleMethods::run(
      *context.options, *context.class_hierarchies, *context.methods);
  context.statistics->log_time("lifecycle_methods", lifecycle_methods_timer);
  LOG(1,
      "Created lifecycle methods in {:.2f}s. Memory used, RSS: {:.2f}GB",
      lifecycle_methods_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  Timer lifecycle_methods_file_path_timer;
  LOG(1, "Setting file paths for life-cycle wrapper methods...");
  for (const auto& [_, lifecycle_method] : lifecycle_methods.methods()) {
    context.positions->set_lifecycle_wrapper_path(lifecycle_method);
  }
  context.statistics->log_time(
      "lifecycle_methods_file_path", lifecycle_methods_file_path_timer);
  LOG(1,
      "Set file paths for lifecycle methods in {:.2f}s. Memory used, RSS: {:.2f}GB",
      lifecycle_methods_file_path_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  // MethodMappings must be constructed after the life-cycle wrapper so that
  // life-cycle methods are added to it.
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
  LOG(1, "Creating user defined shims...");
  Shims shims = ShimGeneration::run(context, method_mappings);
  LOG(1,
      "Created Shims in {:.2f}s. Memory used, RSS: {:.2f}GB",
      shims_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  if (context.options->enable_cross_component_analysis()) {
    Timer intent_routing_analyzer_timer;
    LOG(1, "Running intent routing analyzer...");
    auto intent_routing_analyzer = IntentRoutingAnalyzer::run(
        *context.methods, *context.types, *context.options);
    LOG(1,
        "Created intent routing analyzer in {:.2f}s. Memory used, RSS: {:.2f}MB",
        intent_routing_analyzer_timer.duration_in_seconds(),
        resident_set_size_in_gb());
    shims.add_intent_routing_analyzer(std::move(intent_routing_analyzer));
  }

  Timer call_graph_timer;
  LOG(1, "Building call graph...");
  context.call_graph = std::make_unique<CallGraph>(
      *context.options,
      *context.types,
      *context.class_hierarchies,
      *context.feature_factory,
      *context.heuristics,
      *context.methods,
      *context.fields,
      *context.overrides,
      method_mappings,
      std::move(lifecycle_methods),
      std::move(shims));
  context.statistics->log_time("call_graph", call_graph_timer);
  LOG(1,
      "Built call graph in {:.2f}s. Memory used, RSS: {:.2f}GB",
      call_graph_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  Timer registry_timer;
  LOG(1, "Initializing models...");
  // Model generation takes place within Registry::load() unless the analysis
  // mode does not require it.
  auto registry = Registry::load(
      context,
      *context.options,
      context.options->analysis_mode(),
      std::move(method_mappings));
  context.statistics->log_time("registry_init", registry_timer);
  LOG(1,
      "Initialized {} models, {} field models, and {} literal models in {:.2f}s. Memory used, RSS: {:.2f}GB",
      registry.models_size(),
      registry.field_models_size(),
      registry.literal_models_size(),
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

  // Execute any requested listing commands
  ListingCommands::run(context);

  Timer transforms_timer;
  LOG(1, "Initializing used transform kinds...");
  context.used_kinds = std::make_unique<UsedKinds>(
      UsedKinds::from_rules(*context.rules, *context.transforms_factory));
  LOG(1,
      "Initialized {} source/sink transform kinds and {} propagation transform kinds in {:.2f}s."
      "Memory used, RSS: {:.2f}GB",
      context.used_kinds->source_sink_size(),
      context.used_kinds->propagation_size(),
      transforms_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  Timer kind_pruning_timer;
  LOG(1, "Removing unused Kinds...");
  auto num_removed = context.used_kinds->remove_unused_kinds(
      *context.rules,
      *context.kind_factory,
      *context.methods,
      *context.artificial_methods,
      registry);
  context.statistics->log_time("prune_kinds", rules_timer);
  LOG(1,
      "Removed {} kinds in {:.2f}s.",
      num_removed,
      kind_pruning_timer.duration_in_seconds());

  Timer dependencies_timer;
  LOG(1, "Building dependency graph...");
  context.dependencies = std::make_unique<Dependencies>(
      *context.options,
      *context.heuristics,
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

  auto registry = analyze(context);

  Timer output_timer;
  auto models_path = options.models_output_path();
  LOG(1, "Writing models to `{}`.", models_path.native());
  registry.to_sharded_models_json(models_path);
  context.statistics->log_time("dump_models", output_timer);
  LOG(1, "Wrote models in {:.2f}s.", output_timer.duration_in_seconds());

  if (options.dump_coverage_info()) {
    Timer file_coverage_timer;
    auto file_coverage_output_path = options.file_coverage_output_path();
    LOG(1, "Writing file coverage info to `{}`.", file_coverage_output_path);
    FilesCoverage::compute(registry, *context.positions, context.stores)
        .dump(file_coverage_output_path);
    context.statistics->log_time(
        "dump_file_coverage_info", file_coverage_timer);
    LOG(1,
        "Wrote file coverage info in {:.2f}s.",
        file_coverage_timer.duration_in_seconds());

    Timer rule_coverage_timer;
    auto rule_coverage_output_path = options.rule_coverage_output_path();
    LOG(1, "Writing rule coverage info to `{}`.", rule_coverage_output_path);
    RulesCoverage::compute(registry, *context.rules)
        .dump(rule_coverage_output_path);
    context.statistics->log_time(
        "dump_rule_coverage_info", rule_coverage_timer);
    LOG(1,
        "Wrote rule coverage info in {:.2f}s.",
        rule_coverage_timer.duration_in_seconds());
  }

  auto metadata_path = options.metadata_output_path();
  LOG(1, "Writing metadata to `{}`.", metadata_path.native());
  registry.dump_metadata(/* path */ metadata_path);

  if (options.verify_expected_output()) {
    auto verification_output_path = options.verification_output_path();
    LOG(1,
        "Verifying expected output. Writing results to `{}`",
        verification_output_path.native());
    registry.verify_expected_output(verification_output_path);
  }
}

} // namespace marianatrench

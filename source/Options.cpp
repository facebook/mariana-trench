/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <optional>

#include <boost/algorithm/string.hpp>
#include <fmt/format.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Options.h>

namespace marianatrench {

namespace {

std::string check_path_exists(const std::string& path) {
  if (!boost::filesystem::exists(path)) {
    throw std::invalid_argument(fmt::format("File `{}` does not exist.", path));
  }
  return path;
}

std::string check_directory_exists(const std::string& path) {
  if (!boost::filesystem::is_directory(path)) {
    throw std::invalid_argument(
        fmt::format("Directory `{}` does not exist.", path));
  }
  return path;
}

/* Parse a ';'-separated list of files or directories. */
std::vector<std::string> parse_paths_list(
    const std::string& input,
    const std::optional<std::string>& extension,
    bool check_exist = true) {
  std::vector<std::string> input_paths;
  boost::split(input_paths, input, boost::is_any_of(",;"));

  std::vector<std::string> paths;
  for (const auto& path : input_paths) {
    if (boost::filesystem::is_directory(path)) {
      for (const auto& entry : boost::make_iterator_range(
               boost::filesystem::directory_iterator(path), {})) {
        if (!extension || entry.path().extension() == *extension) {
          paths.push_back(entry.path().native());
        }
      }
    } else if (boost::filesystem::exists(path)) {
      paths.push_back(path);
    } else if (!check_exist) {
      WARNING(2, "Argument path does not exist: `{}`", path);
      paths.push_back(path);
    } else {
      throw std::invalid_argument(
          fmt::format("File `{}` does not exist.", path));
    }
  }
  return paths;
}

std::vector<std::string> parse_search_paths(const std::string& input) {
  std::vector<std::string> paths;
  boost::split(paths, input, boost::is_any_of(",;"));

  for (const auto& path : paths) {
    if (!boost::filesystem::is_directory(path)) {
      throw std::invalid_argument(
          fmt::format("Directory `{}` does not exist.", path));
    }
  }
  return paths;
}

std::vector<ModelGeneratorConfiguration> parse_json_configuration_files(
    const std::vector<std::string>& paths) {
  std::vector<ModelGeneratorConfiguration> result;
  for (const auto& path : paths) {
    Json::Value json = JsonValidation::parse_json_file(path);
    for (const auto& value : JsonValidation::null_or_array(json)) {
      result.push_back(ModelGeneratorConfiguration::from_json(value));
    }
  }
  return result;
}

} // namespace

namespace program_options = boost::program_options;

Options::Options(
    const std::vector<std::string>& models_paths,
    const std::vector<std::string>& field_models_paths,
    const std::vector<std::string>& literal_models_paths,
    const std::vector<std::string>& rules_paths,
    const std::vector<std::string>& lifecycles_paths,
    const std::vector<std::string>& shims_paths,
    const std::string& graphql_metadata_paths,
    const std::vector<std::string>& proguard_configuration_paths,
    bool sequential,
    bool skip_source_indexing,
    bool skip_analysis,
    const std::vector<ModelGeneratorConfiguration>&
        model_generators_configuration,
    const std::vector<std::string>& model_generator_search_paths,
    bool remove_unreachable_code,
    bool emit_all_via_cast_features,
    const std::string& source_root_directory,
    bool enable_cross_component_analysis,
    bool enable_class_intervals,
    ExportOriginsMode export_origins_mode)
    : models_paths_(models_paths),
      field_models_paths_(field_models_paths),
      literal_models_paths_(literal_models_paths),
      rules_paths_(rules_paths),
      lifecycles_paths_(lifecycles_paths),
      shims_paths_(shims_paths),
      graphql_metadata_paths_(graphql_metadata_paths),
      proguard_configuration_paths_(proguard_configuration_paths),
      model_generators_configuration_(model_generators_configuration),
      model_generator_search_paths_(model_generator_search_paths),
      source_root_directory_(source_root_directory),
      sequential_(sequential),
      skip_source_indexing_(skip_source_indexing),
      skip_analysis_(skip_analysis),
      remove_unreachable_code_(remove_unreachable_code),
      disable_parameter_type_overrides_(false),
      disable_global_type_analysis_(false),
      maximum_method_analysis_time_(std::nullopt),
      maximum_source_sink_distance_(10),
      emit_all_via_cast_features_(emit_all_via_cast_features),
      allow_via_cast_features_({}),
      dump_class_hierarchies_(false),
      dump_class_intervals_(false),
      dump_overrides_(false),
      dump_call_graph_(false),
      dump_dependencies_(false),
      dump_methods_(false),
      enable_cross_component_analysis_(enable_cross_component_analysis),
      enable_class_intervals_(enable_class_intervals),
      export_origins_mode_(export_origins_mode) {}

Options::Options(const boost::program_options::variables_map& variables) {
  system_jar_paths_ = parse_paths_list(
      variables["system-jar-paths"].as<std::string>(),
      std::nullopt,
      /* check exist */ false);

  apk_directory_ =
      check_directory_exists(variables["apk-directory"].as<std::string>());
  dex_directory_ =
      check_directory_exists(variables["dex-directory"].as<std::string>());

  if (!variables["models-paths"].empty()) {
    models_paths_ = parse_paths_list(
        variables["models-paths"].as<std::string>(), /* extension */ ".json");
  }
  if (!variables["field-models-paths"].empty()) {
    field_models_paths_ = parse_paths_list(
        variables["field-models-paths"].as<std::string>(), /* extension */ ".json");
  }
  if (!variables["literal-models-paths"].empty()) {
    literal_models_paths_ = parse_paths_list(
        variables["literal-models-paths"].as<std::string>(), /* extension */ ".json");
  }
  rules_paths_ = parse_paths_list(
      variables["rules-paths"].as<std::string>(), /* extension */ ".json");

  if (!variables["lifecycles-paths"].empty()) {
    lifecycles_paths_ = parse_paths_list(
        variables["lifecycles-paths"].as<std::string>(),
        /* extension */ ".json");
  }

  if (!variables["shims-paths"].empty()) {
    shims_paths_ = parse_paths_list(
        variables["shims-paths"].as<std::string>(),
        /* extension */ ".json");
  }

  if (!variables["graphql-metadata-paths"].empty()) {
    graphql_metadata_paths_ = check_path_exists(
        variables["graphql-metadata-paths"].as<std::string>());
  } else {
    graphql_metadata_paths_ = "";
  }

  if (!variables["proguard-configuration-paths"].empty()) {
    proguard_configuration_paths_ = parse_paths_list(
        variables["proguard-configuration-paths"].as<std::string>(),
        /* extension */ ".pro");
  }

  if (!variables["generated-models-directory"].empty()) {
    generated_models_directory_ = check_path_exists(
        variables["generated-models-directory"].as<std::string>());
  }

  generator_configuration_paths_ = parse_paths_list(
      variables["model-generator-configuration-paths"].as<std::string>(),
      /* extension */ ".json");
  model_generators_configuration_ =
      parse_json_configuration_files(generator_configuration_paths_);

  if (!variables["model-generator-search-paths"].empty()) {
    model_generator_search_paths_ = parse_search_paths(
        variables["model-generator-search-paths"].as<std::string>());
  }

  repository_root_directory_ = check_directory_exists(
      variables["repository-root-directory"].as<std::string>());
  source_root_directory_ = check_directory_exists(
      variables["source-root-directory"].as<std::string>());

  if (!variables["source-exclude-directories"].empty()) {
    source_exclude_directories_ = parse_paths_list(
        variables["source-exclude-directories"].as<std::string>(),
        /* extension */ std::nullopt,
        /* check_exist */ false);
  }

  if (!variables["grepo-metadata-path"].empty()) {
    grepo_metadata_path_ =
        check_path_exists(variables["grepo-metadata-path"].as<std::string>());
  }

  apk_path_ = check_path_exists(variables["apk-path"].as<std::string>());
  output_directory_ = boost::filesystem::path(
      check_directory_exists(variables["output-directory"].as<std::string>()));

  sequential_ = variables.count("sequential") > 0;
  skip_source_indexing_ = variables.count("skip-source-indexing") > 0;
  skip_analysis_ = variables.count("skip-analysis") > 0;
  disable_parameter_type_overrides_ =
      variables.count("disable-parameter-type-overrides") > 0;
  disable_global_type_analysis_ =
      variables.count("disable-global-type-analysis") > 0;
  remove_unreachable_code_ = variables.count("remove-unreachable-code") > 0;

  maximum_method_analysis_time_ =
      variables.count("maximum-method-analysis-time") == 0
      ? std::nullopt
      : std::make_optional<int>(
            variables["maximum-method-analysis-time"].as<int>());
  maximum_source_sink_distance_ =
      variables["maximum-source-sink-distance"].as<int>();
  emit_all_via_cast_features_ =
      variables.count("emit-all-via-cast-features") > 0;
  if (!variables["allow-via-cast-feature"].empty()) {
    allow_via_cast_features_ =
        variables["allow-via-cast-feature"].as<std::vector<std::string>>();
  }

  if (!variables["log-method"].empty()) {
    log_methods_ = variables["log-method"].as<std::vector<std::string>>();
  }
  if (!variables["log-method-types"].empty()) {
    log_method_types_ =
        variables["log-method-types"].as<std::vector<std::string>>();
  }
  dump_class_hierarchies_ = variables.count("dump-class-hierarchies") > 0;
  dump_class_intervals_ = variables.count("dump-class-intervals") > 0;
  dump_overrides_ = variables.count("dump-overrides") > 0;
  dump_call_graph_ = variables.count("dump-call-graph") > 0;
  dump_dependencies_ = variables.count("dump-dependencies") > 0;
  dump_methods_ = variables.count("dump-methods") > 0;

  job_id_ = variables.count("job-id") == 0
      ? std::nullopt
      : std::make_optional<std::string>(variables["job-id"].as<std::string>());
  metarun_id_ = variables.count("metarun-id") == 0
      ? std::nullopt
      : std::make_optional<std::string>(
            variables["metarun-id"].as<std::string>());

  enable_cross_component_analysis_ =
      variables.count("enable-cross-component-analysis") > 0;
  enable_class_intervals_ = variables.count("enable-class-intervals") > 0;
  export_origins_mode_ = variables.count("always-export-origins")
      ? ExportOriginsMode::Always
      : ExportOriginsMode::OnlyOnOrigins;
}

void Options::add_options(
    boost::program_options::options_description& options) {
  options.add_options()(
      "system-jar-paths",
      program_options::value<std::string>()->required(),
      "A JSON configuration file with a list of paths to the system jars.");
  options.add_options()(
      "apk-directory",
      program_options::value<std::string>()->required(),
      "The extraced APK obtained by `redex -u`.");
  options.add_options()(
      "dex-directory",
      program_options::value<std::string>()->required(),
      "The extraced DEX obtained by `redex -u`.");

  options.add_options()(
      "models-paths",
      program_options::value<std::string>(),
      "A `;` separated list of models files and directories containing models files.");
  options.add_options()(
      "field-models-paths",
      program_options::value<std::string>(),
      "A `;` separated list of field models files and directories containing field models files.");
  options.add_options()(
      "literal-models-paths",
      program_options::value<std::string>(),
      "A `;` separated list of literal models files and directories containing literal models files.");
  options.add_options()(
      "rules-paths",
      program_options::value<std::string>()->required(),
      "A `;` separated list of rules files and directories containing rules files.");
  options.add_options()(
      "proguard-configuration-paths",
      program_options::value<std::string>(),
      "A `;` separated list of ProGuard configuration files or directories containing ProGuard configuration files.");
  options.add_options()(
      "lifecycles-paths",
      program_options::value<std::string>(),
      "A `;` separated list of files and directories containing life-cycles files.");
  options.add_options()(
      "shims-paths",
      program_options::value<std::string>(),
      "A `;` separated list of files and directories containing shims files.");
  options.add_options()(
      "graphql-metadata-paths",
      program_options::value<std::string>(),
      "A graphql metadata JSON mapping file that will be used to generate asset_xid's for the GraphQL sink.");
  options.add_options()(
      "generated-models-directory",
      program_options::value<std::string>(),
      "Directory where generated models will be stored.");
  options.add_options()(
      "model-generator-configuration-paths",
      program_options::value<std::string>()->required(),
      "A `;` separated list of JSON configuration files each specifying a list of absolute paths to JSON model generators or names of CPP model generators.");
  options.add_options()(
      "model-generator-search-paths",
      program_options::value<std::string>(),
      "A `;` separated list of paths where we look for JSON model generators.");

  options.add_options()(
      "repository-root-directory",
      program_options::value<std::string>()->required(),
      "The root of the repository.");
  options.add_options()(
      "source-root-directory",
      program_options::value<std::string>()->required(),
      "The root where source files for the APK can be found.");
  options.add_options()(
      "source-exclude-directories",
      program_options::value<std::string>(),
      "A `;`-separated list of directories that should be excluded from indexed source files.");
  options.add_options()(
      "grepo-metadata-path",
      program_options::value<std::string>(),
      "A json file containing grepo metadata for source file indexing.");

  options.add_options()(
      "apk-path",
      program_options::value<std::string>()->required(),
      "The APK to analyze.");
  options.add_options()(
      "output-directory",
      program_options::value<std::string>()->required(),
      "Directory to write results in.");

  options.add_options()(
      "sequential", "Run the global fixpoint without parallelization.");
  options.add_options()(
      "skip-source-indexing", "Skip indexing java source files.");
  options.add_options()("skip-analysis", "Skip taint analysis.");
  options.add_options()(
      "disable-parameter-type-overrides",
      "Disable analyzing methods with specific parameter type information.");
  options.add_options()(
      "disable-global-type-analysis",
      "Disable running Redex's global type analysis to infer types.");
  options.add_options()(
      "remove-unreachable-code",
      "Prune unreachable code based on entry points specified in proguard configuration.");
  options.add_options()(
      "maximum-method-analysis-time",
      program_options::value<int>(),
      "Specify number of seconds as a bound. If the analysis of a method takes longer than this then make the method obscure (default taint-in-taint-out).");

  options.add_options()(
      "maximum-source-sink-distance",
      program_options::value<int>(),
      "Limits the distance of sources and sinks from a trace entry point.");
  options.add_options()(
      "emit-all-via-cast-features",
      "Compute and emit all via-cast features. There can be many such features which slows down the analysis so it is disabled by default. Use this to enable it.");
  options.add_options()(
      "allow-via-cast-feature",
      program_options::value<std::vector<std::string>>(),
      "Compute only these via-cast features. Specified as the full type name, e.g. Ljava/lang/Object;. Multiple inputs allowed. Use --emit-all-via-cast-features to allow everything.");

  options.add_options()(
      "log-method",
      program_options::value<std::vector<std::string>>()->multitoken(),
      "Enable logging for the given methods.");
  options.add_options()(
      "log-method-types",
      program_options::value<std::vector<std::string>>()->multitoken(),
      "Enable logging of types for the given methods.");
  options.add_options()(
      "dump-class-hierarchies",
      "Dump the class hierarchies in `class_hierarchies.json`.");
  options.add_options()(
      "dump-class-intervals",
      "Dump the class intervals in `class_intervals.json`. For test/debug only.");
  options.add_options()(
      "dump-overrides", "Dump the override graph in `overrides.json`.");
  options.add_options()(
      "dump-call-graph", "Dump the call graph in `call_graph.json`.");
  options.add_options()(
      "dump-dependencies", "Dump the dependency graph in `dependencies.json`.");
  options.add_options()(
      "dump-methods", "Dump the list of method signatures in `methods.json`.");

  options.add_options()(
      "job-id",
      program_options::value<std::string>(),
      "Identifier for the current analysis run.");
  options.add_options()(
      "metarun-id",
      program_options::value<std::string>(),
      "Identifier for a group of analysis runs.");
  options.add_options()(
      "enable-cross-component-analysis",
      "Compute taint flows across Android components.");
  options.add_options()(
      "enable-class-intervals",
      "Compute and apply class intervals for improved precision.");
  options.add_options()(
      "always-export-origins",
      "Export origin information for all frames instead of only leaves. Used for debugging.");
}

const std::vector<std::string>& Options::models_paths() const {
  return models_paths_;
}

const std::vector<std::string>& Options::field_models_paths() const {
  return field_models_paths_;
}

const std::vector<std::string>& Options::literal_models_paths() const {
  return literal_models_paths_;
}

const std::vector<ModelGeneratorConfiguration>&
Options::model_generators_configuration() const {
  return model_generators_configuration_;
}

const std::vector<std::string>& Options::rules_paths() const {
  return rules_paths_;
}

const std::vector<std::string>& Options::lifecycles_paths() const {
  return lifecycles_paths_;
}

const std::vector<std::string>& Options::shims_paths() const {
  return shims_paths_;
}

const std::string& Options::graphql_metadata_paths() const {
  return graphql_metadata_paths_;
}

const std::vector<std::string>& Options::proguard_configuration_paths() const {
  return proguard_configuration_paths_;
}

const std::optional<std::string>& Options::generated_models_directory() const {
  return generated_models_directory_;
}

const std::vector<std::string>& Options::generator_configuration_paths() const {
  return generator_configuration_paths_;
}

const std::vector<std::string>& Options::model_generator_search_paths() const {
  return model_generator_search_paths_;
}

const std::string& Options::repository_root_directory() const {
  return repository_root_directory_;
}

const std::string& Options::source_root_directory() const {
  return source_root_directory_;
}

const std::vector<std::string>& Options::source_exclude_directories() const {
  return source_exclude_directories_;
}

const std::string& Options::grepo_metadata_path() const {
  return grepo_metadata_path_;
}

const std::vector<std::string>& Options::system_jar_paths() const {
  return system_jar_paths_;
}

const std::string& Options::apk_directory() const {
  return apk_directory_;
}

const std::string& Options::dex_directory() const {
  return dex_directory_;
}

const std::string& Options::apk_path() const {
  return apk_path_;
}

const boost::filesystem::path Options::metadata_output_path() const {
  return output_directory_ / "metadata.json";
}

const boost::filesystem::path Options::removed_symbols_output_path() const {
  return output_directory_ / "removed_symbols.json";
}

const boost::filesystem::path Options::models_output_path() const {
  return output_directory_;
}

const boost::filesystem::path Options::methods_output_path() const {
  return output_directory_ / "methods.json";
}

const boost::filesystem::path Options::call_graph_output_path() const {
  return output_directory_ / "call_graph.json";
}

const boost::filesystem::path Options::class_hierarchies_output_path() const {
  return output_directory_ / "class_hierarchies.json";
}

const boost::filesystem::path Options::class_intervals_output_path() const {
  return output_directory_ / "class_intervals.json";
}

const boost::filesystem::path Options::overrides_output_path() const {
  return output_directory_ / "overrides.json";
}

const boost::filesystem::path Options::dependencies_output_path() const {
  return output_directory_ / "dependencies.json";
}

bool Options::sequential() const {
  return sequential_;
}

bool Options::skip_source_indexing() const {
  return skip_source_indexing_;
}

bool Options::skip_analysis() const {
  return skip_analysis_;
}

bool Options::disable_parameter_type_overrides() const {
  return disable_parameter_type_overrides_;
}

bool Options::disable_global_type_analysis() const {
  return disable_global_type_analysis_;
}

bool Options::remove_unreachable_code() const {
  return remove_unreachable_code_;
}

std::optional<int> Options::maximum_method_analysis_time() const {
  return maximum_method_analysis_time_;
}

int Options::maximum_source_sink_distance() const {
  return maximum_source_sink_distance_;
}

bool Options::emit_all_via_cast_features() const {
  return emit_all_via_cast_features_;
}

const std::vector<std::string>& Options::allow_via_cast_features() const {
  return allow_via_cast_features_;
}

const std::vector<std::string>& Options::log_methods() const {
  return log_methods_;
}

const std::vector<std::string>& Options::log_method_types() const {
  return log_method_types_;
}

bool Options::dump_class_hierarchies() const {
  return dump_class_hierarchies_;
}

bool Options::dump_class_intervals() const {
  return dump_class_intervals_;
}

bool Options::dump_overrides() const {
  return dump_overrides_;
}

bool Options::dump_call_graph() const {
  return dump_call_graph_;
}

bool Options::dump_dependencies() const {
  return dump_dependencies_;
}

bool Options::dump_methods() const {
  return dump_methods_;
}

const std::optional<std::string>& Options::job_id() const {
  return job_id_;
}

const std::optional<std::string>& Options::metarun_id() const {
  return metarun_id_;
}

bool Options::enable_cross_component_analysis() const {
  return enable_cross_component_analysis_;
}

bool Options::enable_class_intervals() const {
  return enable_class_intervals_;
}

ExportOriginsMode Options::export_origins_mode() const {
  return export_origins_mode_;
}

} // namespace marianatrench

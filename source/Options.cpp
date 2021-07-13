/*
 * Copyright (c) Facebook, Inc. and its affiliates.
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
    const std::vector<std::string>& rules_paths,
    const std::vector<std::string>& lifecycles_paths,
    bool sequential,
    bool skip_source_indexing,
    bool skip_model_generation,
    bool enable_global_type_inference,
    const std::string& source_root_directory)
    : models_paths_(models_paths),
      rules_paths_(rules_paths),
      lifecycles_paths_(lifecycles_paths),
      source_root_directory_(source_root_directory),
      sequential_(sequential),
      skip_source_indexing_(skip_source_indexing),
      skip_model_generation_(skip_model_generation),
      disable_parameter_type_overrides_(false),
      enable_global_type_inference_(enable_global_type_inference),
      maximum_source_sink_distance_(10),
      dump_class_hierarchies_(false),
      dump_overrides_(false),
      dump_call_graph_(false),
      dump_dependencies_(false) {}

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
  rules_paths_ = parse_paths_list(
      variables["rules-paths"].as<std::string>(), /* extension */ ".json");

  if (!variables["proguard-configuration-paths"].empty()) {
    proguard_configuration_paths_ = parse_paths_list(
        variables["proguard-configuration-paths"].as<std::string>(),
        /* extension */ ".pro");
  }

  if (!variables["lifecycles-paths"].empty()) {
    lifecycles_paths_ = parse_paths_list(
        variables["lifecycles-paths"].as<std::string>(),
        /* extension */ ".json");
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
        /* extension */ std::nullopt);
  }

  apk_path_ = check_path_exists(variables["apk-path"].as<std::string>());
  output_directory_ = boost::filesystem::path(
      check_directory_exists(variables["output-directory"].as<std::string>()));

  sequential_ = variables.count("sequential") > 0;
  skip_source_indexing_ = variables.count("skip-source-indexing") > 0;
  skip_model_generation_ = variables.count("skip-model-generation") > 0;
  disable_parameter_type_overrides_ =
      variables.count("disable-parameter-type-overrides") > 0;
  enable_global_type_inference_ =
      variables.count("enable-global-type-inference") > 0;

  maximum_source_sink_distance_ =
      variables["maximum-source-sink-distance"].as<int>();

  if (!variables["log-method"].empty()) {
    log_methods_ = variables["log-method"].as<std::vector<std::string>>();
  }
  dump_class_hierarchies_ = variables.count("dump-class-hierarchies") > 0;
  dump_overrides_ = variables.count("dump-overrides") > 0;
  dump_call_graph_ = variables.count("dump-call-graph") > 0;
  dump_dependencies_ = variables.count("dump-dependencies") > 0;
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
  options.add_options()(
      "skip-model-generation", "Skip running model generation.");
  options.add_options()(
      "enable-global-type-inference",
      "Run Redex global analysis for additional type information.");
  options.add_options()(
      "disable-parameter-type-overrides",
      "Disable analyzing methods with specific parameter type information.");

  options.add_options()(
      "maximum-source-sink-distance",
      program_options::value<int>(),
      "Limits the distance of sources and sinks from a trace entry point.");

  options.add_options()(
      "log-method",
      program_options::value<std::vector<std::string>>()->multitoken(),
      "Enable logging for the given methods.");
  options.add_options()(
      "dump-class-hierarchies",
      "Dump the class hierarchies in `class_hierarchies.json`.");
  options.add_options()(
      "dump-overrides", "Dump the override graph in `overrides.json`.");
  options.add_options()(
      "dump-call-graph", "Dump the call graph in `call_graph.json`.");
  options.add_options()(
      "dump-dependencies", "Dump the dependency graph in `dependencies.json`.");
}

const std::vector<std::string>& Options::models_paths() const {
  return models_paths_;
}

const std::vector<ModelGeneratorConfiguration>&
Options::model_generators_configuration() const {
  return model_generators_configuration_;
}

const std::vector<std::string>& Options::rules_paths() const {
  return rules_paths_;
}

const std::vector<std::string>& Options::proguard_configuration_paths() const {
  return proguard_configuration_paths_;
}

const std::vector<std::string>& Options::lifecycles_paths() const {
  return lifecycles_paths_;
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

bool Options::sequential() const {
  return sequential_;
}

bool Options::skip_source_indexing() const {
  return skip_source_indexing_;
}

bool Options::skip_model_generation() const {
  return skip_model_generation_;
}

bool Options::disable_parameter_type_overrides() const {
  return disable_parameter_type_overrides_;
}

bool Options::enable_global_type_inference() const {
  return enable_global_type_inference_;
}

int Options::maximum_source_sink_distance() const {
  return maximum_source_sink_distance_;
}

const std::vector<std::string>& Options::log_methods() const {
  return log_methods_;
}

bool Options::dump_class_hierarchies() const {
  return dump_class_hierarchies_;
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

} // namespace marianatrench

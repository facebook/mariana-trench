/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <optional>

#include <boost/algorithm/string.hpp>
#include <fmt/format.h>

#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Options.h>

namespace marianatrench {

namespace {

std::string check_path_exists(const std::string& path) {
  if (!std::filesystem::exists(path)) {
    throw std::invalid_argument(fmt::format("File `{}` does not exist.", path));
  }
  return path;
}

std::string check_directory_exists(const std::string& path) {
  if (!std::filesystem::is_directory(path)) {
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
    if (std::filesystem::is_directory(path)) {
      for (const auto& entry : boost::make_iterator_range(
               std::filesystem::directory_iterator(path), {})) {
        if (!extension || entry.path().extension() == *extension) {
          paths.push_back(entry.path().native());
        }
      }
    } else if (std::filesystem::exists(path)) {
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
    if (!std::filesystem::is_directory(path)) {
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
    Json::Value json = JsonReader::parse_json_file(path);
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
    ExportOriginsMode export_origins_mode,
    AnalysisMode analysis_mode,
    bool propagate_across_arguments)
    : models_paths_(models_paths),
      field_models_paths_(field_models_paths),
      literal_models_paths_(literal_models_paths),
      rules_paths_(rules_paths),
      lifecycles_paths_(lifecycles_paths),
      shims_paths_(shims_paths),
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
      verify_expected_output_(false),
      maximum_method_analysis_time_(std::nullopt),
      maximum_source_sink_distance_(10),
      emit_all_via_cast_features_(emit_all_via_cast_features),
      allow_via_cast_features_({}),
      dump_class_hierarchies_(false),
      dump_class_intervals_(false),
      dump_overrides_(false),
      dump_call_graph_(false),
      dump_gta_call_graph_(false),
      dump_dependencies_(false),
      dump_methods_(false),
      dump_coverage_info_(false),
      enable_cross_component_analysis_(enable_cross_component_analysis),
      export_origins_mode_(export_origins_mode),
      analysis_mode_(analysis_mode),
      propagate_across_arguments_(propagate_across_arguments) {}

Options::Options(const Json::Value& json) {
  LOG(2, "Arguments: {}", JsonWriter::to_styled_string(json));

  apk_directory_ =
      check_directory_exists(JsonValidation::string(json, "apk-directory"));
  dex_directory_ =
      check_directory_exists(JsonValidation::string(json, "dex-directory"));

  if (json.isMember("system-jar-paths")) {
    system_jar_paths_ = parse_paths_list(
        JsonValidation::string(json, "system-jar-paths"),
        /* extension */ ".json",
        /* check_exists */ false);
  }

  if (json.isMember("models-paths")) {
    models_paths_ = parse_paths_list(
        JsonValidation::string(json, "models-paths"), /* extension */ ".json");
  }

  if (json.isMember("field-models-paths")) {
    field_models_paths_ = parse_paths_list(
        JsonValidation::string(json, "field-models-paths"),
        /* extension */ ".json");
  }

  if (json.isMember("literal-models-paths")) {
    literal_models_paths_ = parse_paths_list(
        JsonValidation::string(json, "literal-models-paths"),
        /* extension */ ".json");
  }

  rules_paths_ = parse_paths_list(
      JsonValidation::string(json, "rules-paths"), /* extension */ ".json");

  if (json.isMember("lifecycles-paths")) {
    lifecycles_paths_ = parse_paths_list(
        JsonValidation::string(json, "lifecycles-paths"),
        /* extension */ ".json");
  }

  if (json.isMember("shims-paths")) {
    shims_paths_ = parse_paths_list(
        JsonValidation::string(json, "shims-paths"),
        /* extension */ ".json");
  }

  if (json.isMember("third-party-library-package-ids-path")) {
    third_party_library_package_ids_path_ = check_path_exists(
        JsonValidation::string(json, "third-party-library-package-ids-path"));
  }

  if (json.isMember("proguard-configuration-paths")) {
    proguard_configuration_paths_ = parse_paths_list(
        JsonValidation::string(json, "proguard-configuration-paths"),
        /* extension */ ".pro");
  }

  if (json.isMember("generated-models-directory")) {
    generated_models_directory_ = check_path_exists(
        JsonValidation::string(json, "generated-models-directory"));
  }

  generator_configuration_paths_ = parse_paths_list(
      JsonValidation::string(json, "model-generator-configuration-paths"),
      /* extension */ ".json");
  model_generators_configuration_ =
      parse_json_configuration_files(generator_configuration_paths_);

  if (json.isMember("model-generator-search-paths")) {
    model_generator_search_paths_ = parse_search_paths(
        JsonValidation::string(json, "model-generator-search-paths"));
  }

  repository_root_directory_ = check_directory_exists(
      JsonValidation::string(json, "repository-root-directory"));
  source_root_directory_ = check_directory_exists(
      JsonValidation::string(json, "source-root-directory"));

  if (json.isMember("source-exclude-directories")) {
    source_exclude_directories_ = parse_paths_list(
        JsonValidation::string(json, "source-exclude-directories"),
        /* extension */ std::nullopt,
        /* check_exist */ false);
  }

  if (json.isMember("buck-target-metadata-path") &&
      json.isMember("grepo-metadata-path")) {
    throw JsonValidationError(
        json,
        std::nullopt,
        "Expected only one of 'buck-target-metadata-path' or 'grepo-metadata-path");
  }

  if (json.isMember("buck-target-metadata-path")) {
    buck_target_metadata_path_ = check_path_exists(
        JsonValidation::string(json, "buck-target-metadata-path"));
  } else if (json.isMember("grepo-metadata-path")) {
    grepo_metadata_path_ =
        check_path_exists(JsonValidation::string(json, "grepo-metadata-path"));
  }

  apk_path_ = check_path_exists(JsonValidation::string(json, "apk-path"));
  output_directory_ = std::filesystem::path(
      check_directory_exists(JsonValidation::string(json, "output-directory")));

  if (json.isMember("sharded-models-directory")) {
    sharded_models_directory_ = std::filesystem::path(check_directory_exists(
        JsonValidation::string(json, "sharded-models-directory")));
  }

  sequential_ = JsonValidation::optional_boolean(json, "sequential", false);
  skip_source_indexing_ =
      JsonValidation::optional_boolean(json, "skip-source-indexing", false);
  skip_analysis_ =
      JsonValidation::optional_boolean(json, "skip-analysis", false);
  remove_unreachable_code_ =
      JsonValidation::optional_boolean(json, "remove-unreachable-code", false);
  disable_parameter_type_overrides_ = JsonValidation::optional_boolean(
      json, "disable-parameter-type-overrides", false);
  disable_global_type_analysis_ = JsonValidation::optional_boolean(
      json, "disable-global-type-analysis", false);
  verify_expected_output_ =
      JsonValidation::optional_boolean(json, "verify-expected-output", false);
  maximum_method_analysis_time_ =
      JsonValidation::optional_integer(json, "maximum-method-analysis-time");

  maximum_source_sink_distance_ =
      JsonValidation::integer(json, "maximum-source-sink-distance");
  emit_all_via_cast_features_ = JsonValidation::optional_boolean(
      json, "emit-all-via-cast-features", false);

  if (json.isMember("allow-via-cast-feature")) {
    for (const auto& value :
         JsonValidation::nonempty_array(json, "allow-via-cast-feature")) {
      allow_via_cast_features_.push_back(JsonValidation::string(value));
    }
  }

  if (json.isMember("log-method")) {
    for (const auto& value :
         JsonValidation::nonempty_array(json, "log-method")) {
      log_methods_.push_back(JsonValidation::string(value));
    }
  }

  if (json.isMember("log-method-types")) {
    for (const auto& value :
         JsonValidation::nonempty_array(json, "log-method-types")) {
      log_method_types_.push_back(JsonValidation::string(value));
    }
  }

  dump_class_hierarchies_ =
      JsonValidation::optional_boolean(json, "dump-class-hierarchies", false);
  dump_class_intervals_ =
      JsonValidation::optional_boolean(json, "dump-class-intervals", false);
  dump_overrides_ =
      JsonValidation::optional_boolean(json, "dump-overrides", false);
  dump_call_graph_ =
      JsonValidation::optional_boolean(json, "dump-call-graph", false);
  dump_gta_call_graph_ =
      JsonValidation::optional_boolean(json, "dump-gta-call-graph", false);
  dump_dependencies_ =
      JsonValidation::optional_boolean(json, "dump-dependencies", false);
  dump_methods_ = JsonValidation::optional_boolean(json, "dump-methods", false);
  dump_coverage_info_ =
      JsonValidation::optional_boolean(json, "dump-coverage-info", false);

  job_id_ = JsonValidation::optional_string(json, "job-id");
  metarun_id_ = JsonValidation::optional_string(json, "metarun-id");

  enable_cross_component_analysis_ = JsonValidation::optional_boolean(
      json, "enable-cross-component-analysis", false);

  export_origins_mode_ =
      JsonValidation::optional_boolean(json, "always-export-origins", false)
      ? ExportOriginsMode::Always
      : ExportOriginsMode::OnlyOnOrigins;

  analysis_mode_ = analysis_mode_from_string(
      JsonValidation::string_or_default(json, "analysis-mode", "normal"));

  propagate_across_arguments_ = JsonValidation::optional_boolean(
      json, "propagate-across-arguments", false);

  if (json.isMember("heuristics")) {
    heuristics_path_ = std::filesystem::path(
        check_path_exists(JsonValidation::string(json, "heuristics")));
  }
}

std::unique_ptr<Options> Options::from_json_file(
    const std::filesystem::path& options_json_path) {
  Json::Value json = JsonReader::parse_json_file(options_json_path);
  JsonValidation::validate_object(json);
  return std::make_unique<Options>(json);
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

const std::optional<std::string>&
Options::third_party_library_package_ids_path() const {
  return third_party_library_package_ids_path_;
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

const std::optional<std::string>& Options::buck_target_metadata_path() const {
  return buck_target_metadata_path_;
}

const std::optional<std::string>& Options::grepo_metadata_path() const {
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

const std::filesystem::path Options::metadata_output_path() const {
  return output_directory_ / "metadata.json";
}

const std::filesystem::path Options::removed_symbols_output_path() const {
  return output_directory_ / "removed_symbols.json";
}

const std::filesystem::path Options::models_output_path() const {
  return output_directory_;
}

const std::filesystem::path Options::methods_output_path() const {
  return output_directory_ / "methods.json";
}

const std::filesystem::path Options::call_graph_output_path() const {
  return output_directory_;
}

const std::filesystem::path Options::gta_call_graph_output_path() const {
  return output_directory_ / "gta_call_graph.json";
}

const std::filesystem::path Options::class_hierarchies_output_path() const {
  return output_directory_ / "class_hierarchies.json";
}

const std::filesystem::path Options::class_intervals_output_path() const {
  return output_directory_ / "class_intervals.json";
}

const std::filesystem::path Options::overrides_output_path() const {
  return output_directory_ / "overrides.json";
}

const std::filesystem::path Options::dependencies_output_path() const {
  return output_directory_;
}

const std::filesystem::path Options::file_coverage_output_path() const {
  return output_directory_ / "file_coverage.txt";
}

const std::filesystem::path Options::rule_coverage_output_path() const {
  return output_directory_ / "rule_coverage.json";
}

const std::filesystem::path Options::verification_output_path() const {
  return output_directory_ / "verification.json";
}

const std::optional<std::filesystem::path> Options::sharded_models_directory()
    const {
  return sharded_models_directory_;
}

const std::optional<std::filesystem::path> Options::overrides_input_path()
    const {
  if (!sharded_models_directory_.has_value()) {
    return std::nullopt;
  }
  return *sharded_models_directory_ / "overrides.json";
}

const std::optional<std::filesystem::path>
Options::class_hierarchies_input_path() const {
  if (!sharded_models_directory_.has_value()) {
    return std::nullopt;
  }
  return *sharded_models_directory_ / "class_hierarchies.json";
}

const std::optional<std::filesystem::path> Options::class_intervals_input_path()
    const {
  if (!sharded_models_directory_.has_value()) {
    return std::nullopt;
  }
  return *sharded_models_directory_ / "class_intervals.json";
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

bool Options::verify_expected_output() const {
  return verify_expected_output_;
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

bool Options::dump_gta_call_graph() const {
  return dump_gta_call_graph_;
}

bool Options::dump_dependencies() const {
  return dump_dependencies_;
}

bool Options::dump_methods() const {
  return dump_methods_;
}

bool Options::dump_coverage_info() const {
  return dump_coverage_info_;
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

ExportOriginsMode Options::export_origins_mode() const {
  return export_origins_mode_;
}

AnalysisMode Options::analysis_mode() const {
  return analysis_mode_;
}

bool Options::propagate_across_arguments() const {
  return propagate_across_arguments_;
}

const std::optional<std::filesystem::path> Options::heuristics_path() const {
  return heuristics_path_;
}

} // namespace marianatrench

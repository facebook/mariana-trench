/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <boost/program_options.hpp>
#include <json/json.h>

#include <mariana-trench/AnalysisMode.h>
#include <mariana-trench/ExportOriginsMode.h>
#include <mariana-trench/Heuristics.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/model-generator/ModelGeneratorConfiguration.h>

namespace marianatrench {

class Options final {
 public:
  explicit Options(
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
      const std::string& source_root_directory = ".",
      bool enable_cross_component_analysis = false,
      ExportOriginsMode export_origins_mode = ExportOriginsMode::Always,
      AnalysisMode analysis_mode = AnalysisMode::Normal,
      bool propagate_across_arguments = false);

  explicit Options(const Json::Value& json);

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Options)

  static std::unique_ptr<Options> from_json_file(
      const std::filesystem::path& options_json_path);

  const std::vector<std::string>& models_paths() const;
  const std::vector<std::string>& field_models_paths() const;
  const std::vector<std::string>& literal_models_paths() const;
  const std::vector<ModelGeneratorConfiguration>&
  model_generators_configuration() const;
  const std::vector<std::string>& rules_paths() const;
  const std::vector<std::string>& lifecycles_paths() const;
  const std::vector<std::string>& shims_paths() const;
  const std::string& graphql_metadata_paths() const;
  const std::optional<std::string>& third_party_library_package_ids_path()
      const;
  const std::vector<std::string>& proguard_configuration_paths() const;
  const std::optional<std::string>& generated_models_directory() const;

  const std::vector<std::string>& generator_configuration_paths() const;
  const std::vector<std::string>& model_generator_search_paths() const;

  const std::string& repository_root_directory() const;
  const std::string& source_root_directory() const;
  const std::vector<std::string>& source_exclude_directories() const;
  const std::optional<std::string>& buck_target_metadata_path() const;
  const std::optional<std::string>& grepo_metadata_path() const;

  const std::vector<std::string>& system_jar_paths() const;

  const std::string& apk_directory() const;
  const std::string& dex_directory() const;

  const std::string& apk_path() const;

  const std::filesystem::path metadata_output_path() const;
  const std::filesystem::path removed_symbols_output_path() const;
  const std::filesystem::path models_output_path() const;
  const std::filesystem::path methods_output_path() const;
  const std::filesystem::path call_graph_output_path() const;
  const std::filesystem::path gta_call_graph_output_path() const;
  const std::filesystem::path class_hierarchies_output_path() const;
  const std::filesystem::path class_intervals_output_path() const;
  const std::filesystem::path overrides_output_path() const;
  const std::filesystem::path dependencies_output_path() const;
  const std::filesystem::path file_coverage_output_path() const;
  const std::filesystem::path rule_coverage_output_path() const;
  const std::filesystem::path verification_output_path() const;

  const std::optional<std::filesystem::path> sharded_models_directory() const;
  const std::optional<std::filesystem::path> overrides_input_path() const;
  const std::optional<std::filesystem::path> class_hierarchies_input_path()
      const;
  const std::optional<std::filesystem::path> class_intervals_input_path() const;

  bool sequential() const;
  bool skip_source_indexing() const;
  bool skip_analysis() const;
  bool remove_unreachable_code() const;
  bool disable_parameter_type_overrides() const;
  bool disable_global_type_analysis() const;
  bool verify_expected_output() const;
  std::optional<int> maximum_method_analysis_time() const;

  int maximum_source_sink_distance() const;
  bool emit_all_via_cast_features() const;
  const std::vector<std::string>& allow_via_cast_features() const;

  const std::vector<std::string>& log_methods() const;
  const std::vector<std::string>& log_method_types() const;
  bool dump_class_hierarchies() const;
  bool dump_class_intervals() const;
  bool dump_overrides() const;
  bool dump_call_graph() const;
  bool dump_gta_call_graph() const;
  bool dump_dependencies() const;
  bool dump_methods() const;
  bool dump_coverage_info() const;

  const std::optional<std::string>& job_id() const;
  const std::optional<std::string>& metarun_id() const;

  bool enable_cross_component_analysis() const;
  ExportOriginsMode export_origins_mode() const;
  AnalysisMode analysis_mode() const;
  bool propagate_across_arguments() const;

  const std::optional<std::filesystem::path> heuristics_path() const;

  // Listing command flags
  bool list_all_rules() const;
  bool list_all_model_generators() const;
  bool list_all_model_generators_in_rules() const;
  bool list_all_kinds() const;
  bool list_all_kinds_in_rules() const;
  bool list_all_filters() const;
  bool list_all_features() const;
  bool list_all_lifecycles() const;

 private:
  std::vector<std::string> models_paths_;
  std::vector<std::string> field_models_paths_;
  std::vector<std::string> literal_models_paths_;
  std::vector<std::string> rules_paths_;
  std::vector<std::string> lifecycles_paths_;
  std::vector<std::string> shims_paths_;
  std::string graphql_metadata_paths_;
  std::optional<std::string> third_party_library_package_ids_path_;
  std::vector<std::string> proguard_configuration_paths_;

  std::vector<std::string> generator_configuration_paths_;
  std::vector<ModelGeneratorConfiguration> model_generators_configuration_;
  std::vector<std::string> model_generator_search_paths_;

  std::optional<std::string> generated_models_directory_;

  std::string repository_root_directory_;
  std::string source_root_directory_;
  std::vector<std::string> source_exclude_directories_;
  std::optional<std::string> buck_target_metadata_path_;
  std::optional<std::string> grepo_metadata_path_;

  std::vector<std::string> system_jar_paths_;
  std::string apk_directory_;
  std::string dex_directory_;

  std::string apk_path_;
  std::filesystem::path output_directory_;

  std::optional<std::filesystem::path> sharded_models_directory_;

  bool sequential_;
  bool skip_source_indexing_;
  bool skip_analysis_;
  bool remove_unreachable_code_;
  bool disable_parameter_type_overrides_;
  bool disable_global_type_analysis_;
  bool verify_expected_output_;
  std::optional<int> maximum_method_analysis_time_;

  int maximum_source_sink_distance_;
  bool emit_all_via_cast_features_;
  std::vector<std::string> allow_via_cast_features_;

  std::vector<std::string> log_methods_;
  std::vector<std::string> log_method_types_;
  bool dump_class_hierarchies_;
  bool dump_class_intervals_;
  bool dump_overrides_;
  bool dump_call_graph_;
  bool dump_gta_call_graph_;
  bool dump_dependencies_;
  bool dump_methods_;
  bool dump_coverage_info_;

  std::optional<std::string> job_id_;
  std::optional<std::string> metarun_id_;

  bool enable_cross_component_analysis_;
  ExportOriginsMode export_origins_mode_;
  AnalysisMode analysis_mode_;
  bool propagate_across_arguments_;

  std::optional<std::filesystem::path> heuristics_path_;
  
  // Listing command flags
  bool list_all_rules_;
  bool list_all_model_generators_;
  bool list_all_model_generators_in_rules_;
  bool list_all_kinds_;
  bool list_all_kinds_in_rules_;
  bool list_all_filters_;
  bool list_all_features_;
  bool list_all_lifecycles_;
};

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <json/json.h>

#include <mariana-trench/ExportOriginsMode.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/model-generator/ModelGeneratorConfiguration.h>

namespace marianatrench {

class Options final {
 public:
  explicit Options(
      const std::vector<std::string>& models_paths,
      const std::vector<std::string>& field_models_paths,
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
      bool enable_class_intervals = false,
      ExportOriginsMode export_origins_mode = ExportOriginsMode::Always);

  explicit Options(const boost::program_options::variables_map& variables);

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Options)

  static void add_options(boost::program_options::options_description& options);

  const std::vector<std::string>& models_paths() const;
  const std::vector<std::string>& field_models_paths() const;
  const std::vector<ModelGeneratorConfiguration>&
  model_generators_configuration() const;
  const std::vector<std::string>& rules_paths() const;
  const std::vector<std::string>& lifecycles_paths() const;
  const std::vector<std::string>& shims_paths() const;
  const std::string& graphql_metadata_paths() const;
  const std::vector<std::string>& proguard_configuration_paths() const;
  const std::optional<std::string>& generated_models_directory() const;

  const std::vector<std::string>& generator_configuration_paths() const;
  const std::vector<std::string>& model_generator_search_paths() const;

  const std::string& repository_root_directory() const;
  const std::string& source_root_directory() const;
  const std::vector<std::string>& source_exclude_directories() const;
  const std::string& grepo_metadata_path() const;

  const std::vector<std::string>& system_jar_paths() const;

  const std::string& apk_directory() const;
  const std::string& dex_directory() const;

  const std::string& apk_path() const;

  const boost::filesystem::path metadata_output_path() const;
  const boost::filesystem::path removed_symbols_output_path() const;
  const boost::filesystem::path models_output_path() const;
  const boost::filesystem::path methods_output_path() const;
  const boost::filesystem::path call_graph_output_path() const;
  const boost::filesystem::path class_hierarchies_output_path() const;
  const boost::filesystem::path class_intervals_output_path() const;
  const boost::filesystem::path overrides_output_path() const;
  const boost::filesystem::path dependencies_output_path() const;

  bool sequential() const;
  bool skip_source_indexing() const;
  bool skip_analysis() const;
  bool disable_parameter_type_overrides() const;
  bool disable_global_type_analysis() const;
  bool remove_unreachable_code() const;
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
  bool dump_dependencies() const;
  bool dump_methods() const;

  const std::optional<std::string>& job_id() const;
  const std::optional<std::string>& metarun_id() const;

  bool enable_cross_component_analysis() const;
  bool enable_class_intervals() const;
  ExportOriginsMode export_origins_mode() const;

 private:
  std::vector<std::string> models_paths_;
  std::vector<std::string> field_models_paths_;
  std::vector<std::string> rules_paths_;
  std::vector<std::string> lifecycles_paths_;
  std::vector<std::string> shims_paths_;
  std::string graphql_metadata_paths_;
  std::vector<std::string> proguard_configuration_paths_;

  std::vector<std::string> generator_configuration_paths_;
  std::vector<ModelGeneratorConfiguration> model_generators_configuration_;
  std::vector<std::string> model_generator_search_paths_;

  std::optional<std::string> generated_models_directory_;

  std::string repository_root_directory_;
  std::string source_root_directory_;
  std::vector<std::string> source_exclude_directories_;
  std::string grepo_metadata_path_;

  std::vector<std::string> system_jar_paths_;
  std::string apk_directory_;
  std::string dex_directory_;

  std::string apk_path_;
  boost::filesystem::path output_directory_;

  bool sequential_;
  bool skip_source_indexing_;
  bool skip_analysis_;
  bool remove_unreachable_code_;
  bool disable_parameter_type_overrides_;
  bool disable_global_type_analysis_;
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
  bool dump_dependencies_;
  bool dump_methods_;

  std::optional<std::string> job_id_;
  std::optional<std::string> metarun_id_;

  bool enable_cross_component_analysis_;
  bool enable_class_intervals_;
  ExportOriginsMode export_origins_mode_;
};

} // namespace marianatrench

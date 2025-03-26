/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/CachedModelsContext.h>
#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/Overrides.h>

namespace marianatrench {

namespace {

std::unordered_map<const Method*, std::unordered_set<const Method*>>
read_overrides(const Options& options, Methods& methods) {
  auto overrides_file = options.overrides_input_path();
  if (!std::filesystem::exists(*overrides_file)) {
    throw std::runtime_error(
        "Overrides file must exist when sharded input models are provided.");
  }

  LOG(1, "Reading overrides from `{}`", overrides_file->native());
  auto overrides_json = JsonReader::parse_json_file(*overrides_file);
  return Overrides::from_json(overrides_json, methods);
}

std::unordered_map<const DexType*, std::unordered_set<const DexType*>>
read_class_hierarchies(const Options& options) {
  auto class_hierarchies_file = options.class_hierarchies_input_path();
  if (!std::filesystem::exists(*class_hierarchies_file)) {
    throw std::runtime_error(
        "Class hierarchies file must exist when sharded input models are provided.");
  }

  LOG(1,
      "Reading class hierarchies from `{}`",
      class_hierarchies_file->native());

  auto class_hierarchies_json =
      JsonReader::parse_json_file(*class_hierarchies_file);
  return ClassHierarchies::from_json(class_hierarchies_json);
}

std::unordered_map<const DexType*, ClassIntervals::Interval>
read_class_intervals(const Options& options) {
  auto class_intervals_file = options.class_intervals_input_path();
  if (!std::filesystem::exists(*class_intervals_file)) {
    throw std::runtime_error("Class intervals file must exist.");
  }

  LOG(1, "Reading class intervals from `{}`", class_intervals_file->native());

  auto class_intervals_json =
      JsonReader::parse_json_file(*class_intervals_file);
  return ClassIntervals::from_json(class_intervals_json);
}

} // namespace

CachedModelsContext::CachedModelsContext(
    Context& context,
    const Options& options)
    : is_cleared_(false) {
  auto analysis_mode = options.analysis_mode();
  if (analysis_mode == AnalysisMode::Normal) {
    // Normal mode does not need cached/preloaded models.
    return;
  }

  auto sharded_models_directory = options.sharded_models_directory();
  if (!sharded_models_directory.has_value()) {
    throw std::runtime_error(fmt::format(
        "Analysis mode `{}` requires sharded models to be provided.",
        analysis_mode_to_string(analysis_mode)));
  }

  models_.emplace(
      Registry::from_sharded_models_json(context, *sharded_models_directory));
  overrides_ = read_overrides(options, *context.methods);
  class_hierarchy_ = read_class_hierarchies(options);

  if (analysis_mode == AnalysisMode::Replay) {
    class_intervals_ = read_class_intervals(options);
  } else {
    // Outside of replay mode (i.e. cached models), do NOT read class intervals.
    // Interweaving intervals of external methods with the APK's methods is not
    // supported yet.
    LOG(1,
        "Not reading class intervals for analysis mode {}",
        analysis_mode_to_string(analysis_mode));
  }
}

void CachedModelsContext::clear() {
  overrides_.clear();
  class_hierarchy_.clear();
  class_intervals_.clear();
  models_.reset();
  is_cleared_ = true;
}

} // namespace marianatrench

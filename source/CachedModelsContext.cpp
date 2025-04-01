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

CachedModelsContext::OverridesMap read_overrides(
    const Options& options,
    Methods& methods) {
  auto overrides_file = options.overrides_input_path();
  if (!std::filesystem::exists(*overrides_file)) {
    throw std::runtime_error(
        "Overrides file must exist when sharded input models are provided.");
  }

  LOG(1, "Reading overrides from `{}`", overrides_file->native());
  auto overrides_json = JsonReader::parse_json_file(*overrides_file);
  return Overrides::from_json(overrides_json, methods);
}

CachedModelsContext::ClassHierarchiesMap read_class_hierarchies(
    const Options& options) {
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

CachedModelsContext::ClassIntervalsMap read_class_intervals(
    const Options& options) {
  auto class_intervals_file = options.class_intervals_input_path();
  if (!std::filesystem::exists(*class_intervals_file)) {
    throw std::runtime_error("Class intervals file must exist.");
  }

  LOG(1, "Reading class intervals from `{}`", class_intervals_file->native());

  auto class_intervals_json =
      JsonReader::parse_json_file(*class_intervals_file);
  return ClassIntervals::from_json(class_intervals_json);
}

Registry read_sharded_models(
    Context& context,
    const std::filesystem::path& path) {
  LOG(1, "Reading models from sharded JSON files...");

  ConcurrentMap<const Method*, Model> models;
  ConcurrentMap<const Field*, FieldModel> field_models;
  ConcurrentMap<std::string, LiteralModel> literal_models;

  // A path with no redundant directory separators, current directory (dot) or
  // parent directory (dot-dot) elements.
  auto directory_name = std::filesystem::canonical(path.string()).filename();

  // Class intervals are not collapsed in replay mode. When models are replayed,
  // the intervals in it are expected to correspond to what was loaded.
  bool replay_mode = context.options->analysis_mode() == AnalysisMode::Replay;
  LOG(1,
      "Class intervals will{} be collapsed. Replay mode: {}",
      replay_mode ? " not" : "",
      replay_mode);

  auto from_json_line = [&context, &directory_name, &models, &replay_mode](
                            const Json::Value& value) -> void {
    JsonValidation::validate_object(value);
    if (value.isMember("method")) {
      try {
        const auto* method = Method::from_json(value["method"], context);
        mt_assert(method != nullptr);
        auto model = Model::from_json(value, context);

        if (!replay_mode) {
          // Indicate that the source of these models is
          // `Options::sharded_models_directory()`
          model.make_sharded_model_generators(
              /* identifier */ directory_name.string());
          model.collapse_class_intervals();
        }

        models.emplace(method, model);
      } catch (const JsonValidationError& e) {
        WARNING(1, "Unable to parse model `{}`: {}", value, e.what());
      }
    } else {
      // TODO(T176362886): Support parsing field and literal models from JSON.
      ERROR(1, "Unrecognized model type in JSON: `{}`", value.toStyledString());
    }
  };

  JsonReader::read_sharded_json_files(path, "model@", from_json_line);

  return Registry(
      context,
      std::move(models),
      std::move(field_models),
      std::move(literal_models));
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

  models_.emplace(read_sharded_models(context, *sharded_models_directory));
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

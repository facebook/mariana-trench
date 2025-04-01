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
}

void CachedModelsContext::clear() {
  models_.reset();
  is_cleared_ = true;
}

} // namespace marianatrench

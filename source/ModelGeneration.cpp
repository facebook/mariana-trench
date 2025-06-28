/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <json/json.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Dependencies.h>
#include <mariana-trench/EventLogger.h>
#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/ModelGeneration.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Timer.h>
#include <mariana-trench/model-generator/BroadcastReceiverGenerator.h>
#include <mariana-trench/model-generator/BuilderPatternGenerator.h>
#include <mariana-trench/model-generator/ContentProviderGenerator.h>
#include <mariana-trench/model-generator/DFASourceGenerator.h>
#include <mariana-trench/model-generator/JoinOverrideGenerator.h>
#include <mariana-trench/model-generator/JsonModelGenerator.h>
#include <mariana-trench/model-generator/ManifestSourceGenerator.h>
#include <mariana-trench/model-generator/ModelGenerator.h>
#include <mariana-trench/model-generator/ModelGeneratorConfiguration.h>
#include <mariana-trench/model-generator/ModelGeneratorName.h>
#include <mariana-trench/model-generator/ModelGeneratorNameFactory.h>
#include <mariana-trench/model-generator/ServiceSourceGenerator.h>
#include <mariana-trench/model-generator/TaintInTaintOutGenerator.h>
#include <mariana-trench/model-generator/TaintInTaintThisGenerator.h>

namespace marianatrench {

namespace {

std::unordered_map<const ModelGeneratorName*, std::unique_ptr<ModelGenerator>>
make_model_generators(
    const Registry* MT_NULLABLE preloaded_models,
    Context& context) {
  std::unordered_map<const ModelGeneratorName*, std::unique_ptr<ModelGenerator>>
      generators = ModelGeneration::make_builtin_model_generators(
          preloaded_models, context);
  // Find JSON model generators in search path.
  for (const auto& path : context.options->model_generator_search_paths()) {
    LOG(3, "Searching for model generators in `{}`...", path);
    for (auto& entry : std::filesystem::recursive_directory_iterator(path)) {
      if (entry.path().extension() == ".json") {
        // TODO(T153463464): We no longer accept legacy .json files for models.
        // Note that there could be rule definition files, lifecycle definition
        // files, etc. that still use the .json extension. In the future, we
        // should use the extension to differentiate between these file types.
        WARNING(
            1,
            "Not parsing `{}`. .json files are deprecated. Use .models instead.",
            entry.path());
        continue;
      }

      if (entry.path().extension() != ".models") {
        continue;
      }

      auto path_copy = entry.path();
      auto identifier = path_copy.replace_extension("").filename().string();
      const auto* name =
          context.model_generator_name_factory->create(identifier);

      try {
        Json::Value json = JsonReader::parse_json_file(entry.path());

        if (!json.isObject()) {
          throw ModelGeneratorError(
              fmt::format(
                  "Unable to parse `{}` as a valid models JSON.",
                  entry.path()));
        }

        auto [_, inserted] = generators.emplace(
            name,
            std::make_unique<JsonModelGenerator>(JsonModelGenerator::from_json(
                name, context, entry.path(), json)));
        if (!inserted) {
          auto error = fmt::format(
              "Duplicate model generator {} defined at {}",
              identifier,
              entry.path());
          throw ModelGeneratorError(error);
        }
        LOG(3, "Found model generator `{}`", identifier);
      } catch (const JsonValidationError& e) {
        auto error = fmt::format(
            "Unable to parse generator at `{}`: {}", entry.path(), e.what());
        LOG(1, error);
        EventLogger::log_event(
            "model_generator_error",
            fmt::format("{}\n{}", error, e.what()),
            /* verbosity_level */ 1);
      }
    }
  }

  return generators;
}

} // namespace

ModelGeneratorError::ModelGeneratorError(const std::string& message)
    : std::invalid_argument(message) {}

#ifndef MARIANA_TRENCH_FACEBOOK_BUILD
std::unordered_map<const ModelGeneratorName*, std::unique_ptr<ModelGenerator>>
ModelGeneration::make_builtin_model_generators(
    const Registry* MT_NULLABLE preloaded_models,
    Context& context) {
  std::vector<std::unique_ptr<ModelGenerator>> builtin_generators;
  builtin_generators.push_back(
      std::make_unique<BroadcastReceiverGenerator>(context));
  builtin_generators.push_back(
      std::make_unique<ContentProviderGenerator>(context));
  builtin_generators.push_back(
      std::make_unique<ServiceSourceGenerator>(context));
  builtin_generators.push_back(
      std::make_unique<TaintInTaintThisGenerator>(preloaded_models, context));
  builtin_generators.push_back(
      std::make_unique<TaintInTaintOutGenerator>(preloaded_models, context));
  builtin_generators.push_back(
      std::make_unique<BuilderPatternGenerator>(context));
  builtin_generators.push_back(
      std::make_unique<JoinOverrideGenerator>(context));
  builtin_generators.push_back(
      std::make_unique<ManifestSourceGenerator>(context));
  builtin_generators.push_back(std::make_unique<DFASourceGenerator>(context));

  std::unordered_map<const ModelGeneratorName*, std::unique_ptr<ModelGenerator>>
      builtin_generator_map;
  for (auto& generator : builtin_generators) {
    const auto* name = generator->name();
    builtin_generator_map.emplace(name, std::move(generator));
  }
  return builtin_generator_map;
}
#endif

ModelGeneratorResult ModelGeneration::run(
    Context& context,
    const Registry* MT_NULLABLE preloaded_models,
    const MethodMappings& method_mappings) {
  const auto& options = *context.options;

  const auto& generated_models_directory = options.generated_models_directory();

  if (generated_models_directory) {
    LOG(2,
        "Removing existing model generators under `{}`...",
        *generated_models_directory);
    for (auto& file :
         std::filesystem::directory_iterator(*generated_models_directory)) {
      const auto& file_path = file.path();
      if (std::filesystem::is_regular_file(file_path) &&
          boost::ends_with(file_path.filename().string(), ".json")) {
        std::filesystem::remove(file_path);
      }
    }
  }

  auto generator_definitions = make_model_generators(preloaded_models, context);
  std::vector<std::unique_ptr<ModelGenerator>> model_generators;
  const auto& configuration_entries = options.model_generators_configuration();

  std::vector<std::string> nonexistent_model_generators;
  for (const auto& entry : configuration_entries) {
    const std::string& identifier = entry.name();
    LOG(2, "Found model generator: `{}`", identifier);
    const auto* name = context.model_generator_name_factory->get(identifier);
    if (name == nullptr) {
      nonexistent_model_generators.push_back(identifier);
      continue;
    }

    auto iterator = generator_definitions.find(name);
    if (iterator == generator_definitions.end()) {
      bool generator_exists = std::any_of(
          model_generators.begin(),
          model_generators.end(),
          [name](const auto& generator) { return generator->name() == name; });
      if (!generator_exists) {
        nonexistent_model_generators.push_back(identifier);
      }
    } else {
      model_generators.push_back(std::move(iterator->second));
      generator_definitions.erase(iterator);
    }
  }

  if (!nonexistent_model_generators.empty()) {
    throw std::invalid_argument(
        fmt::format(
            "Model generator(s) {} either do not exist or couldn't be parsed.",
            boost::algorithm::join(nonexistent_model_generators, ", ")));
  }

  std::vector<Model> generated_models;
  std::vector<FieldModel> generated_field_models;
  std::size_t iteration = 0;

  for (const auto& model_generator : model_generators) {
    Timer generator_timer;
    LOG(1,
        "Running model generator `{}` ({}/{})",
        show(model_generator->name()),
        ++iteration,
        model_generators.size());

    auto [models, field_models] = model_generator->run_optimized(
        *context.methods, method_mappings, *context.fields);

    // Remove models for the `null` method
    models.erase(
        std::remove_if(
            models.begin(),
            models.end(),
            [](const Model& model) { return !model.method(); }),
        models.end());
    field_models.erase(
        std::remove_if(
            field_models.begin(),
            field_models.end(),
            [](const FieldModel& field_model) { return !field_model.field(); }),
        field_models.end());

    generated_models.insert(
        generated_models.end(), models.begin(), models.end());
    generated_field_models.insert(
        generated_field_models.end(), field_models.begin(), field_models.end());

    LOG(1,
        "  Model generator `{}` generated {} models in {:.2f}s.",
        show(model_generator->name()),
        models.size(),
        generator_timer.duration_in_seconds());

    if (generated_models_directory) {
      // Persist models to file.
      Timer generator_output_timer;
      LOG(2,
          "Writing generated models to `{}`...",
          *generated_models_directory);

      // Merge models
      auto registry =
          Registry(context, models, field_models, /* literal_models */ {});
      JsonWriter::write_json_file(
          *generated_models_directory + "/" +
              model_generator->name()->identifier() + ".json",
          registry.models_to_json());

      LOG(2,
          "Wrote {} generated models to `{}` in {:.2f}s.",
          registry.models_size(),
          *generated_models_directory,
          generator_output_timer.duration_in_seconds());
    }
  }

  return {/* method_models */ generated_models,
          /* field_models */ generated_field_models};
}

std::unordered_map<const ModelGeneratorName*, std::filesystem::path>
ModelGeneration::get_json_model_generator_paths(Context& context) {
  std::unordered_map<const ModelGeneratorName*, std::filesystem::path> paths;
  
  // Find JSON model generators in search paths
  for (const auto& path : context.options->model_generator_search_paths()) {
    for (auto& entry : std::filesystem::recursive_directory_iterator(path)) {
      if (entry.path().extension() == ".json") {
        // Skip deprecated .json files
        continue;
      }

      if (entry.path().extension() != ".models") {
        continue;
      }

      auto path_copy = entry.path();
      auto identifier = path_copy.replace_extension("").filename().string();
      const auto* name =
          context.model_generator_name_factory->create(identifier);

      paths.emplace(name, entry.path());
    }
  }

  return paths;
}

} // namespace marianatrench

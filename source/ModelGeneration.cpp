/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <memory>
#include <vector>

#include <json/json.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Dependencies.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/ModelGeneration.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Timer.h>
#include <mariana-trench/model-generator/JsonModelGenerator.h>
#include <mariana-trench/model-generator/ModelGeneratorConfiguration.h>
#include <mariana-trench/model-generator/NoJoinOverridesGenerator.h>
#include <mariana-trench/model-generator/ProviderSourceGenerator.h>
#include <mariana-trench/model-generator/RandomSourceGenerator.h>
#include <mariana-trench/model-generator/RepeatingAlarmSinkGenerator.h>
#include <mariana-trench/model-generator/ServiceSourceGenerator.h>
#include <mariana-trench/model-generator/StructuredLoggerSinkGenerator.h>
#include <mariana-trench/model-generator/TouchEventSinkGenerator.h>

namespace marianatrench {

namespace {

std::map<std::string, std::unique_ptr<ModelGenerator>> make_model_generators(
    Context& context) {
  std::vector<std::unique_ptr<ModelGenerator>> builtin_generators;
  builtin_generators.push_back(
      std::make_unique<NoJoinOverridesGenerator>(context));
  builtin_generators.push_back(
      std::make_unique<ProviderSourceGenerator>(context));
  builtin_generators.push_back(
      std::make_unique<RandomSourceGenerator>(context));
  builtin_generators.push_back(
      std::make_unique<RepeatingAlarmSinkGenerator>(context));
  builtin_generators.push_back(
      std::make_unique<ServiceSourceGenerator>(context));
  builtin_generators.push_back(
      std::make_unique<StructuredLoggerSinkGenerator>(context));
  builtin_generators.push_back(
      std::make_unique<TouchEventSinkGenerator>(context));

  std::map<std::string, std::unique_ptr<ModelGenerator>> generators;
  for (auto& generator : builtin_generators) {
    auto name = generator->name();
    generators.emplace(name, std::move(generator));
  }

  // Find JSON model generators in search path.
  for (const auto& path : context.options->model_generator_search_paths()) {
    LOG(3, "Searching for model generators in `{}`...", path);
    for (auto& entry : boost::filesystem::directory_iterator(path)) {
      if (entry.path().extension() != ".json") {
        continue;
      }

      auto path_copy = entry.path();
      auto name = path_copy.replace_extension("").filename().string();

      try {
        auto [_, inserted] = generators.emplace(
            name,
            std::make_unique<JsonModelGenerator>(name, context, entry.path()));
        if (!inserted) {
          auto error = fmt::format(
              "Duplicate model generator {} defined at {}", name, entry.path());
          throw std::invalid_argument(error);
        }
        LOG(3, "Found model generator `{}`", name);
      } catch (const JsonValidationError&) {
        LOG(3, "Unable to parse generator at `{}`", path);
      }
    }
  }

  return generators;
}

} // namespace

std::vector<Model> ModelGeneration::run(Context& context) {
  const auto& options = *context.options;

  const auto& generated_models_directory = options.generated_models_directory();

  if (generated_models_directory) {
    LOG(2,
        "Removing existing model generators under `{}`...",
        *generated_models_directory);
    for (auto& file :
         boost::filesystem::directory_iterator(*generated_models_directory)) {
      const auto& file_path = file.path();
      if (boost::filesystem::is_regular_file(file_path) &&
          boost::ends_with(file_path.filename().string(), ".json")) {
        boost::filesystem::remove(file_path);
      }
    }
  }

  auto builtin_generators = make_model_generators(context);
  std::vector<std::unique_ptr<ModelGenerator>> model_generators;
  const auto& configuration_entries = options.model_generators_configuration();
  // We assume that the path to a json model generator is relative to the
  // path of the json configuration file that specifies model generators
  auto directory_of_json_model_generators =
      boost::filesystem::path{options.generator_configuration_path()}
          .parent_path();
  // TODO(T84659873): deprecate path lookup. We want to lookup by name instead
  // to to support search path.
  for (const auto& entry : configuration_entries) {
    if (entry.kind() == ModelGeneratorConfiguration::Kind::JSON) {
      auto relative_path = boost::filesystem::path{entry.name_or_path()};
      auto absolute_path = (directory_of_json_model_generators / relative_path);
      LOG(2, "Found JSON model generator: `{}`", absolute_path);

      model_generators.push_back(std::make_unique<JsonModelGenerator>(
          /* generator name */ relative_path.filename().stem().string(),
          context,
          /* absolute path */ absolute_path));
    } else if (entry.kind() == ModelGeneratorConfiguration::Kind::CPP) {
      const std::string& name = entry.name_or_path();
      LOG(2, "Found CPP model generator: `{}`", name);

      auto iterator = builtin_generators.find(name);
      if (iterator == builtin_generators.end()) {
        bool generator_exists = std::any_of(
            model_generators.begin(),
            model_generators.end(),
            [&](const auto& generator) { return generator->name() == name; });
        if (!generator_exists) {
          ERROR(1, "Model generator `{}` does not exist.", name);
        }
      } else {
        model_generators.push_back(std::move(iterator->second));
        builtin_generators.erase(iterator);
      }
    } else {
      mt_unreachable();
    }
  }

  std::vector<Model> generated_models;
  std::size_t iteration = 0;

  std::unique_ptr<MethodMappings> method_mappings = nullptr;
  if (context.options->optimized_model_generation()) {
    LOG(1,
        "Building method mappings for model generation over {} methods",
        context.methods->size());
    Timer method_mapping_timer;
    method_mappings = std::make_unique<MethodMappings>(*context.methods);
    LOG(1,
        "Generated method mappings in {:.2f}s",
        method_mapping_timer.duration_in_seconds());
  }

  for (const auto& model_generator : model_generators) {
    Timer generator_timer;
    LOG(1,
        "Running model generator `{}` ({}/{})",
        model_generator->name(),
        ++iteration,
        model_generators.size());

    std::vector<Model> models;
    if (method_mappings != nullptr) {
      models =
          model_generator->run_optimized(*context.methods, *method_mappings);
    } else {
      models = model_generator->run(*context.methods);
    }

    // Remove models for the `null` method
    models.erase(
        std::remove_if(
            models.begin(),
            models.end(),
            [](const Model& model) { return !model.method(); }),
        models.end());

    generated_models.insert(
        generated_models.end(), models.begin(), models.end());

    LOG(2,
        "Generated {} models in {:.2f}s.",
        models.size(),
        generator_timer.duration_in_seconds());

    if (generated_models_directory) {
      // Persist models to file.
      Timer generator_output_timer;
      LOG(2,
          "Writing generated models to `{}`...",
          *generated_models_directory);

      // Merge models
      auto registry = Registry(context, models);
      JsonValidation::write_json_file(
          *generated_models_directory + "/" + model_generator->name() + ".json",
          registry.models_to_json());

      LOG(2,
          "Wrote {} generated models to `{}` in {:.2f}s.",
          registry.models_size(),
          *generated_models_directory,
          generator_output_timer.duration_in_seconds());
    }
  }

  return generated_models;
}

} // namespace marianatrench

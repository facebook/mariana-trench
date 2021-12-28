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
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/ModelGeneration.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Timer.h>
#include <mariana-trench/model-generator/ContentProviderGenerator.h>
#include <mariana-trench/model-generator/JsonModelGenerator.h>
#include <mariana-trench/model-generator/ModelGeneratorConfiguration.h>
#include <mariana-trench/model-generator/ServiceSourceGenerator.h>
#include <mariana-trench/model-generator/TaintInTaintOutGenerator.h>
#include <mariana-trench/model-generator/TaintInTaintThisGenerator.h>

namespace marianatrench {

namespace {

std::map<std::string, std::unique_ptr<ModelGenerator>> make_model_generators(
    Context& context) {
  std::map<std::string, std::unique_ptr<ModelGenerator>> generators =
      ModelGeneration::make_builtin_model_generators(context);
  // Find JSON model generators in search path.
  for (const auto& path : context.options->model_generator_search_paths()) {
    LOG(3, "Searching for model generators in `{}`...", path);
    for (auto& entry : boost::filesystem::recursive_directory_iterator(path)) {
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

#ifndef MARIANA_TRENCH_FACEBOOK_BUILD
std::map<std::string, std::unique_ptr<ModelGenerator>>
ModelGeneration::make_builtin_model_generators(Context& context) {
  std::vector<std::unique_ptr<ModelGenerator>> builtin_generators;
  builtin_generators.push_back(
      std::make_unique<ContentProviderGenerator>(context));
  builtin_generators.push_back(
      std::make_unique<ServiceSourceGenerator>(context));
  builtin_generators.push_back(
      std::make_unique<TaintInTaintThisGenerator>(context));
  builtin_generators.push_back(
      std::make_unique<TaintInTaintOutGenerator>(context));

  std::map<std::string, std::unique_ptr<ModelGenerator>> builtin_generator_map;
  for (auto& generator : builtin_generators) {
    auto name = generator->name();
    builtin_generator_map.emplace(name, std::move(generator));
  }
  return builtin_generator_map;
}
#endif

ModelGeneratorResult ModelGeneration::run(Context& context) {
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

  std::vector<std::string> nonexistent_model_generators;
  for (const auto& entry : configuration_entries) {
    const std::string& name = entry.name();
    LOG(2, "Found model generator: `{}`", name);

    auto iterator = builtin_generators.find(name);
    if (iterator == builtin_generators.end()) {
      bool generator_exists = std::any_of(
          model_generators.begin(),
          model_generators.end(),
          [&](const auto& generator) { return generator->name() == name; });
      if (!generator_exists) {
        nonexistent_model_generators.push_back(name);
      }
    } else {
      model_generators.push_back(std::move(iterator->second));
      builtin_generators.erase(iterator);
    }
  }

  if (!nonexistent_model_generators.empty()) {
    throw std::invalid_argument(fmt::format(
        "Model generator(s) {} either do not exist or couldn't be parsed.",
        boost::algorithm::join(nonexistent_model_generators, ", ")));
  }

  std::vector<Model> generated_models;
  std::vector<FieldModel> generated_field_models;
  std::size_t iteration = 0;

  LOG(1,
      "Building method mappings for model generation over {} methods",
      context.methods->size());
  Timer method_mapping_timer;
  std::unique_ptr<MethodMappings> method_mappings =
      std::make_unique<MethodMappings>(*context.methods);
  LOG(1,
      "Generated method mappings in {:.2f}s",
      method_mapping_timer.duration_in_seconds());

  for (const auto& model_generator : model_generators) {
    Timer generator_timer;
    LOG(1,
        "Running model generator `{}` ({}/{})",
        model_generator->name(),
        ++iteration,
        model_generators.size());

    auto [models, field_models] = model_generator->run_optimized(
        *context.methods, *method_mappings, *context.fields);

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
      auto registry = Registry(context, models, field_models);
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

  return {
      /* method_models */ generated_models,
      /* field_models */ generated_field_models};
}

} // namespace marianatrench

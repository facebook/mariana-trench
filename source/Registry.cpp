/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdio>
#include <filesystem>
#include <fstream>

#include <boost/algorithm/string/predicate.hpp>
#include <fmt/format.h>
#include <json/value.h>

#include <ConcurrentContainers.h>
#include <DexAnnotation.h>
#include <sparta/WorkQueue.h>

#include <mariana-trench/ArtificialMethods.h>
#include <mariana-trench/Constants.h>
#include <mariana-trench/Filesystem.h>
#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/MethodMappings.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/ModelGeneration.h>
#include <mariana-trench/ModelValidator.h>
#include <mariana-trench/OperatingSystem.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Positions.h>
#include <mariana-trench/Registry.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/RulesCoverage.h>
#include <mariana-trench/Statistics.h>
#include <mariana-trench/model-generator/ModelGeneratorNameFactory.h>

namespace marianatrench {

namespace {

Registry run_model_generators(
    Context& context,
    const MethodMappings& method_mappings,
    const std::optional<Registry>& cached_registry) {
  Timer generation_timer;
  LOG(1, "Generating models...");
  auto model_generator_result =
      ModelGeneration::run(context, cached_registry, method_mappings);
  context.statistics->log_time("models_generation", generation_timer);
  LOG(1,
      "Generated {} models and {} field models in {:.2f}s. Memory used, RSS: {:.2f}GB",
      model_generator_result.method_models.size(),
      model_generator_result.field_models.size(),
      generation_timer.duration_in_seconds(),
      resident_set_size_in_gb());

  // Add models for artificial methods.
  auto models = context.artificial_methods->models(context);
  model_generator_result.method_models.insert(
      model_generator_result.method_models.end(), models.begin(), models.end());

  return Registry(
      context,
      model_generator_result.method_models,
      model_generator_result.field_models,
      /* literal_models */ {});
}

// Load models json input
Registry from_models_file(Context& context, const Options& options) {
  std::vector<Model> models;
  for (const auto& models_path : options.models_paths()) {
    auto models_json = JsonReader::parse_json_file(models_path);
    for (const auto& value : JsonValidation::null_or_array(models_json)) {
      const auto* method = Method::from_json(value["method"], context);
      mt_assert(method != nullptr);
      models.emplace_back(Model::from_config_json(method, value, context));
    }
  }

  std::vector<FieldModel> field_models;
  for (const auto& field_models_path : options.field_models_paths()) {
    auto field_model_json = JsonReader::parse_json_file(field_models_path);
    for (const auto& value : JsonValidation::null_or_array(field_model_json)) {
      const auto* field = Field::from_json(value["field"], context);
      mt_assert(field != nullptr);
      field_models.emplace_back(
          FieldModel::from_config_json(field, value, context));
    }
  }

  std::vector<LiteralModel> literal_models;
  for (const auto& literal_models_path : options.literal_models_paths()) {
    auto literal_model_json = JsonReader::parse_json_file(literal_models_path);
    for (const auto& value :
         JsonValidation::null_or_array(literal_model_json)) {
      literal_models.emplace_back(
          LiteralModel::from_config_json(value, context));
    }
  }

  return Registry(context, models, field_models, literal_models);
}

Registry from_sharded_models(
    Context& context,
    const std::filesystem::path& path) {
  LOG(1, "Reading models from sharded JSON files...");

  ConcurrentMap<const Method*, Model> models;
  ConcurrentMap<const Field*, FieldModel> field_models;
  ConcurrentMap<std::string, LiteralModel> literal_models;

  // A path with no redundant directory separators, current directory (dot) or
  // parent directory (dot-dot) elements.
  auto directory_name = std::filesystem::canonical(path).filename();

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

Registry::Registry(Context& context, bool create_default_models)
    : context_(context) {
  if (!create_default_models) {
    // Empty registry with only context_ initialized
    return;
  }
  add_default_models();
}

Registry::Registry(
    Context& context,
    const std::vector<Model>& models,
    const std::vector<FieldModel>& field_models,
    const std::vector<LiteralModel>& literal_models)
    : context_(context) {
  for (const auto& model : models) {
    join_with(model);
  }
  for (const auto& field_model : field_models) {
    join_with(field_model);
  }
  for (const auto& literal_model : literal_models) {
    join_with(literal_model);
  }
}

Registry::Registry(
    Context& context,
    const Json::Value& models_value,
    const Json::Value& field_models_value,
    const Json::Value& literal_models_value)
    : context_(context) {
  for (const auto& value : JsonValidation::null_or_array(models_value)) {
    const auto* method = Method::from_json(value["method"], context);
    mt_assert(method != nullptr);
    join_with(Model::from_config_json(method, value, context));
  }
  for (const auto& value : JsonValidation::null_or_array(field_models_value)) {
    const auto* field = Field::from_json(value["field"], context);
    mt_assert(field != nullptr);
    join_with(FieldModel::from_config_json(field, value, context));
  }
  for (const auto& value :
       JsonValidation::null_or_array(literal_models_value)) {
    join_with(LiteralModel::from_config_json(value, context));
  }
}

Registry::Registry(
    Context& context,
    ConcurrentMap<const Method*, Model>&& models,
    ConcurrentMap<const Field*, FieldModel>&& field_models,
    ConcurrentMap<std::string, LiteralModel>&& literal_models)
    : context_(context),
      models_(std::move(models)),
      field_models_(std::move(field_models)),
      literal_models_(std::move(literal_models)) {}

Registry Registry::load(
    Context& context,
    const Options& options,
    AnalysisMode analysis_mode,
    MethodMappings method_mappings) {
  switch (analysis_mode) {
    case AnalysisMode::Normal: {
      auto registry = run_model_generators(
          context, method_mappings, /* cached_registry */ std::nullopt);
      registry.join_with(from_models_file(context, options));
      registry.add_default_models();
      return registry;
    }
    case AnalysisMode::CachedModels: {
      auto sharded_models_directory = options.sharded_models_directory();
      if (!sharded_models_directory.has_value()) {
        throw std::runtime_error(fmt::format(
            "Analysis mode `{}` requires sharded models to be provided.",
            analysis_mode_to_string(analysis_mode)));
      }
      auto cached_registry = std::make_optional<Registry>(
          from_sharded_models(context, *sharded_models_directory));
      auto registry =
          run_model_generators(context, method_mappings, cached_registry);
      registry.join_with(from_models_file(context, options));
      registry.join_with(*cached_registry);
      registry.add_default_models();
      return registry;
    }
    case AnalysisMode::Replay: {
      auto sharded_models_directory = options.sharded_models_directory();
      if (!sharded_models_directory.has_value()) {
        throw std::runtime_error(fmt::format(
            "Analysis mode `{}` requires sharded models to be provided.",
            analysis_mode_to_string(analysis_mode)));
      }
      return from_sharded_models(context, *sharded_models_directory);
    }
    default:
      mt_unreachable();
  }
}

void Registry::add_default_models() {
  auto queue = sparta::work_queue<const Method*>(
      [this](const Method* method) {
        models_.insert(std::make_pair(method, Model(method, context_)));
      },
      sparta::parallel::default_num_threads());
  for (const auto* method : *context_.methods) {
    queue.add_item(method);
  }
  queue.run_all();
}

bool Registry::has_model(const Method* method) const {
  return models_.find(method) != models_.end();
}

Model Registry::get(const Method* method) const {
  if (!method) {
    throw std::runtime_error("Trying to get model for the `null` method");
  }

  try {
    return models_.at(method);
  } catch (const std::out_of_range&) {
    throw std::runtime_error(fmt::format(
        "Trying to get model for untracked method `{}`.", method->show()));
  }
}

FieldModel Registry::get(const Field* field) const {
  if (!field) {
    throw std::runtime_error("Trying to get model for the `null` field");
  }

  try {
    return field_models_.at(field);
  } catch (const std::out_of_range&) {
    return FieldModel(field);
  }
}

void Registry::set(const Model& model) {
  models_.insert_or_assign(std::make_pair(model.method(), model));
}

LiteralModel Registry::match_literal(const std::string_view literal) const {
  LiteralModel result_model;
  for (const auto& [pattern, model] : UnorderedIterable(literal_models_)) {
    if (model.matches(literal)) {
      result_model.join_with(model);
    }
  }
  return result_model;
}

std::size_t Registry::models_size() const {
  return models_.size();
}

std::size_t Registry::field_models_size() const {
  return field_models_.size();
}

std::size_t Registry::issues_size() const {
  std::size_t result = 0;
  for (const auto& entry : UnorderedIterable(models_)) {
    result += entry.second.issues().size();
  }
  return result;
}

void Registry::join_with(const Model& model) {
  const auto* method = model.method();
  mt_assert(method);
  auto iterator = models_.find(method);
  if (iterator != models_.end()) {
    iterator->second.join_with(model);
  } else {
    models_.insert(std::make_pair(method, model));
  }
}

void Registry::join_with(const FieldModel& field_model) {
  const auto* field = field_model.field();
  mt_assert(field);
  auto iterator = field_models_.find(field);
  if (iterator != field_models_.end()) {
    iterator->second.join_with(field_model);
  } else {
    field_models_.insert(std::make_pair(field, field_model));
  }
}

void Registry::join_with(const LiteralModel& literal_model) {
  const std::optional<std::string> pattern{literal_model.pattern()};
  mt_assert(pattern);
  auto iterator = literal_models_.find(*pattern);
  if (iterator != literal_models_.end()) {
    iterator->second.join_with(literal_model);
  } else {
    literal_models_.emplace(*pattern, literal_model);
  }
}

void Registry::join_with(const Registry& other) {
  for (const auto& other_model : UnorderedIterable(other.models_)) {
    join_with(other_model.second);
  }
  for (const auto& other_field_model : UnorderedIterable(other.field_models_)) {
    join_with(other_field_model.second);
  }
  for (const auto& other_literal_model :
       UnorderedIterable(other.literal_models_)) {
    join_with(other_literal_model.second);
  }
}

void Registry::dump_metadata(const std::filesystem::path& path) const {
  auto value = Json::Value(Json::objectValue);

  auto codes = Json::Value(Json::objectValue);
  for (const auto* rule : *context_.rules) {
    codes[std::to_string(rule->code())] = Json::Value(rule->name());
  }
  value["codes"] = codes;

  auto rules = Json::Value(Json::arrayValue);
  for (const auto* rule : *context_.rules) {
    rules.append(rule->to_json());
  }
  value["rules"] = rules;

  auto statistics = context_.statistics->to_json();
  statistics["issues"] = Json::Value(static_cast<Json::UInt64>(issues_size()));
  statistics["methods_analyzed"] =
      Json::Value(static_cast<Json::UInt64>(models_.size()));
  statistics["methods_without_code"] = Json::Value(static_cast<Json::UInt64>(
      unordered_count_if(models_, [](const auto& model) {
        return model.first->get_code() == nullptr;
      })));
  statistics["methods_skipped"] = Json::Value(static_cast<Json::UInt64>(
      unordered_count_if(models_, [](const auto& model) {
        return model.second.skip_analysis();
      })));
  value["stats"] = statistics;

  value["filename_spec"] = Json::Value("model@*.json");
  value["repo_root"] =
      Json::Value(context_.options->repository_root_directory());
  value["root"] = Json::Value(context_.options->source_root_directory());
  value["tool"] = Json::Value("mariana_trench");
  value["version"] = Json::Value("0.2");

  JsonWriter::write_json_file(path, value);
}

std::string Registry::dump_models() const {
  auto writer = JsonWriter::compact_writer();
  std::stringstream string;
  string << "// @";
  string << "generated\n";
  for (const auto& model : UnorderedIterable(models_)) {
    writer->write(model.second.to_json(context_), &string);
    string << "\n";
  }
  for (const auto& field_model : UnorderedIterable(field_models_)) {
    writer->write(field_model.second.to_json(context_), &string);
    string << "\n";
  }
  for (const auto& literal_model : UnorderedIterable(literal_models_)) {
    writer->write(literal_model.second.to_json(context_), &string);
    string << "\n";
  }
  return string.str();
}

Json::Value Registry::models_to_json() const {
  auto models_value = Json::Value(Json::objectValue);
  models_value["models"] = Json::Value(Json::arrayValue);
  for (auto model : UnorderedIterable(models_)) {
    models_value["models"].append(model.second.to_json(context_));
  }
  models_value["field_models"] = Json::Value(Json::arrayValue);
  for (auto field_model : UnorderedIterable(field_models_)) {
    models_value["field_models"].append(field_model.second.to_json(context_));
  }
  models_value["literal_models"] = Json::Value(Json::arrayValue);
  for (const auto& literal_model : UnorderedIterable(literal_models_)) {
    models_value["literal_models"].append(
        literal_model.second.to_json(context_));
  }
  return models_value;
}

void Registry::to_sharded_models_json(
    const std::filesystem::path& path,
    const std::size_t batch_size) const {
  std::vector<Model> models;
  for (const auto& model : UnorderedIterable(models_)) {
    models.push_back(model.second);
  }

  std::vector<FieldModel> field_models;
  for (const auto& field_model : UnorderedIterable(field_models_)) {
    field_models.push_back(field_model.second);
  }

  std::vector<LiteralModel> literal_models;
  for (const auto& literal_model : UnorderedIterable(literal_models_)) {
    literal_models.push_back(literal_model.second);
  }

  std::size_t total_elements =
      models.size() + field_models.size() + literal_models.size();

  auto to_json_line = [&](std::size_t i) -> Json::Value {
    mt_assert(i < total_elements);
    if (i < models.size()) {
      return models[i].to_json(context_);
    } else if (i < models.size() + field_models.size()) {
      return field_models[i - models.size()].to_json(context_);
    } else {
      return literal_models[i - models.size() - field_models.size()].to_json(
          context_);
    }
  };

  JsonWriter::write_sharded_json_files(
      path, batch_size, total_elements, "model@", to_json_line);
}

void Registry::dump_file_coverage_info(
    const std::filesystem::path& output_path) const {
  std::unordered_set<std::string> covered_paths;
  for (const auto& [method, model] : UnorderedIterable(models_)) {
    if (method->get_code() == nullptr || model.skip_analysis()) {
      continue;
    }

    const auto* path = context_.positions->get_path(method->dex_method());
    if (path) {
      covered_paths.insert(*path);
    }
  }

  std::ofstream output_file;
  output_file.open(output_path, std::ios_base::out);
  if (!output_file.is_open()) {
    ERROR(
        1, "Unable to write file coverage info to `{}`.", output_path.native());
    return;
  }

  for (const auto& path : covered_paths) {
    output_file << path << std::endl;
  }

  output_file.close();
}

void Registry::dump_rule_coverage_info(
    const std::filesystem::path& output_path) const {
  std::unordered_set<const Kind*> used_sources;
  std::unordered_set<const Kind*> used_sinks;
  std::unordered_set<const Transform*> used_transforms;

  for (const auto& [_method, model] : UnorderedIterable(models_)) {
    auto source_kinds = model.source_kinds();
    used_sources.insert(source_kinds.begin(), source_kinds.end());

    auto sink_kinds = model.sink_kinds();
    used_sinks.insert(sink_kinds.begin(), sink_kinds.end());

    auto transforms = model.local_transform_kinds();
    used_transforms.insert(transforms.begin(), transforms.end());
  }

  for (const auto& [_field, model] : UnorderedIterable(field_models_)) {
    auto source_kinds = model.sources().kinds();
    used_sources.insert(source_kinds.begin(), source_kinds.end());

    auto sink_kinds = model.sinks().kinds();
    used_sinks.insert(sink_kinds.begin(), sink_kinds.end());
  }

  for (const auto& [_literal, model] : UnorderedIterable(literal_models_)) {
    auto source_kinds = model.sources().kinds();
    used_sources.insert(source_kinds.begin(), source_kinds.end());
  }

  auto rule_coverage = RulesCoverage::create(
      *(context_.rules), used_sources, used_sinks, used_transforms);
  JsonWriter::write_json_file(output_path, rule_coverage.to_json());
}

void Registry::verify_expected_output(
    const std::filesystem::path& test_output_path) const {
  auto results = Json::Value(Json::arrayValue);

  for (const auto& [method, model] : UnorderedIterable(models_)) {
    auto model_validators = ModelValidators::from_method(method);
    if (!model_validators) {
      continue;
    }
    results.append(model_validators->validate(model).to_json());
  }

  JsonWriter::write_json_file(test_output_path, results);
}

} // namespace marianatrench

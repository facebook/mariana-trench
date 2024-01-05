/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <cstdio>

#include <boost/algorithm/string/predicate.hpp>
#include <fmt/format.h>
#include <json/value.h>

#include <sparta/WorkQueue.h>

#include <mariana-trench/Constants.h>
#include <mariana-trench/Filesystem.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Registry.h>
#include <mariana-trench/Rules.h>
#include <mariana-trench/Statistics.h>

namespace marianatrench {

Registry::Registry(Context& context) : context_(context) {
  auto queue = sparta::work_queue<const Method*>(
      [this, &context](const Method* method) { set(Model(method, context)); },
      sparta::parallel::default_num_threads());
  for (const auto* method : *context.methods) {
    queue.add_item(method);
  }
  queue.run_all();
}

Registry::Registry(
    Context& context,
    const std::vector<Model>& models,
    const std::vector<FieldModel>& field_models)
    : context_(context) {
  for (const auto& model : models) {
    join_with(model);
  }
  for (const auto& field_model : field_models) {
    join_with(field_model);
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
    join_with(Model::from_json(method, value, context));
  }
  for (const auto& value : JsonValidation::null_or_array(field_models_value)) {
    const auto* field = Field::from_json(value["field"], context);
    mt_assert(field != nullptr);
    join_with(FieldModel::from_json(field, value, context));
  }
  for (const auto& value :
       JsonValidation::null_or_array(literal_models_value)) {
    join_with(LiteralModel::from_json(value, context));
  }
}

Registry Registry::load(
    Context& context,
    const Options& options,
    const std::vector<Model>& generated_models,
    const std::vector<FieldModel>& generated_field_models) {
  // Create a registry with the generated models
  Registry registry(context, generated_models, generated_field_models);

  // TODO(T157984454): We should unify the loading of models from files and
  // loading model generators, so we can use unique "model generator" names.

  // Load models json input
  for (const auto& models_path : options.models_paths()) {
    registry.join_with(Registry(
        context,
        /* models_value */ JsonValidation::parse_json_file(models_path),
        /* field_models_value */ Json::Value(Json::arrayValue),
        /* literal_models_value */ Json::Value(Json::arrayValue)));
  }
  for (const auto& field_models_path : options.field_models_paths()) {
    registry.join_with(Registry(
        context,
        /* models_value */ Json::Value(Json::arrayValue),
        /* field_models_value */
        JsonValidation::parse_json_file(field_models_path),
        /* literal_models_value */ Json::Value(Json::arrayValue)));
  }
  for (const auto& literal_models_path : options.literal_models_paths()) {
    registry.join_with(Registry(
        context,
        /* models_value */ Json::Value(Json::arrayValue),
        /* field_models_value */ Json::Value(Json::arrayValue),
        /* literal_models_value */
        JsonValidation::parse_json_file(literal_models_path)));
  }

  // Add a default model for methods that don't have one
  registry.add_default_models();

  return registry;
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
  for (const auto& [pattern, model] : literal_models_) {
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
  for (const auto& entry : models_) {
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
  for (const auto& other_model : other.models_) {
    join_with(other_model.second);
  }
  for (const auto& other_field_model : other.field_models_) {
    join_with(other_field_model.second);
  }
  for (const auto& other_literal_model : other.literal_models_) {
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
      std::count_if(models_.begin(), models_.end(), [](const auto& model) {
        return model.first->get_code() == nullptr;
      })));
  statistics["methods_skipped"] = Json::Value(static_cast<Json::UInt64>(
      std::count_if(models_.begin(), models_.end(), [](const auto& model) {
        return model.second.skip_analysis();
      })));
  value["stats"] = statistics;

  value["filename_spec"] = Json::Value("model@*.json");
  value["repo_root"] =
      Json::Value(context_.options->repository_root_directory());
  value["root"] = Json::Value(context_.options->source_root_directory());
  value["tool"] = Json::Value("mariana_trench");
  value["version"] = Json::Value("0.2");

  JsonValidation::write_json_file(path, value);
}

std::string Registry::dump_models() const {
  auto writer = JsonValidation::compact_writer();
  std::stringstream string;
  string << "// @";
  string << "generated\n";
  for (const auto& model : models_) {
    writer->write(model.second.to_json(context_), &string);
    string << "\n";
  }
  for (const auto& field_model : field_models_) {
    writer->write(field_model.second.to_json(context_), &string);
    string << "\n";
  }
  for (const auto& literal_model : literal_models_) {
    writer->write(literal_model.second.to_json(context_), &string);
    string << "\n";
  }
  return string.str();
}

Json::Value Registry::models_to_json() const {
  auto models_value = Json::Value(Json::objectValue);
  models_value["models"] = Json::Value(Json::arrayValue);
  for (auto model : models_) {
    models_value["models"].append(model.second.to_json(context_));
  }
  models_value["field_models"] = Json::Value(Json::arrayValue);
  for (auto field_model : field_models_) {
    models_value["field_models"].append(field_model.second.to_json(context_));
  }
  models_value["literal_models"] = Json::Value(Json::arrayValue);
  for (const auto& literal_model : literal_models_) {
    models_value["literal_models"].append(
        literal_model.second.to_json(context_));
  }
  return models_value;
}

void Registry::dump_models(
    const std::filesystem::path& path,
    const std::size_t batch_size) const {
  // Remove existing model files under this directory.
  for (auto& file : std::filesystem::directory_iterator(path)) {
    const auto& file_path = file.path();
    if (std::filesystem::is_regular_file(file_path) &&
        boost::starts_with(file_path.filename().string(), "model@")) {
      std::filesystem::remove(file_path);
    }
  }

  std::vector<Model> models;
  for (const auto& model : models_) {
    models.push_back(model.second);
  }

  std::vector<FieldModel> field_models;
  for (const auto& field_model : field_models_) {
    field_models.push_back(field_model.second);
  }

  std::vector<LiteralModel> literal_models;
  for (const auto& literal_model : literal_models_) {
    literal_models.push_back(literal_model.second);
  }

  const auto total_batch =
      (models_.size() + field_models_.size() + literal_models_.size()) /
          batch_size +
      1;
  const auto padded_total_batch = fmt::format("{:0>5}", total_batch);

  auto queue = sparta::work_queue<std::size_t>(
      [&](std::size_t batch) {
        // Construct a valid sharded file name for SAPP.
        const auto padded_batch = fmt::format("{:0>5}", batch);
        const auto batch_path = path /
            ("model@" + padded_batch + "-of-" + padded_total_batch + ".json");

        std::ofstream batch_stream(batch_path.native(), std::ios_base::out);
        if (!batch_stream.is_open()) {
          ERROR(1, "Unable to write models to `{}`.", batch_path.native());
          return;
        }
        batch_stream << "// @"
                     << "generated\n";

        // Write the current batch of models to file.
        auto writer = JsonValidation::compact_writer();
        for (std::size_t i = batch_size * batch; i < batch_size * (batch + 1) &&
             i < models.size() + field_models.size() + literal_models.size();
             i++) {
          if (i < models.size()) {
            writer->write(models[i].to_json(context_), &batch_stream);
          } else if (i < models.size() + field_models.size()) {
            writer->write(
                field_models[i - models.size()].to_json(context_),
                &batch_stream);
          } else {
            writer->write(
                literal_models[i - models.size() - field_models.size()].to_json(
                    context_),
                &batch_stream);
          }
          batch_stream << "\n";
        }
        batch_stream.close();
      },
      sparta::parallel::default_num_threads());

  for (std::size_t batch = 0; batch < total_batch; batch++) {
    queue.add_item(batch);
  }
  queue.run_all();

  LOG(1, "Wrote models to {} shards.", total_batch);
}

} // namespace marianatrench

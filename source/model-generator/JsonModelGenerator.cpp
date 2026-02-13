/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/EventLogger.h>
#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/model-generator/JsonModelGenerator.h>
#include <mariana-trench/model-generator/ModelGeneratorNameFactory.h>

namespace marianatrench {

JsonModelGeneratorItem::JsonModelGeneratorItem(
    const ModelGeneratorName* name,
    Context& context,
    std::unique_ptr<AllOfMethodConstraint> constraint,
    ModelTemplate model_template,
    int verbosity)
    : MethodVisitorModelGenerator(name, context),
      constraint_(std::move(constraint)),
      model_template_(std::move(model_template)),
      verbosity_(verbosity) {}

std::vector<Model> JsonModelGeneratorItem::emit_method_models_filtered(
    const MethodHashedSet& methods) {
  return this->run_impl(methods.elements().begin(), methods.elements().end());
}

std::vector<Model> JsonModelGeneratorItem::visit_method(
    const Method* method) const {
  std::vector<Model> models;
  if (constraint_->satisfy(method)) {
    LOG(verbosity_,
        "Method `{}{}` satisfies all constraints in json model generator {}",
        method->is_static() ? "(static) " : "",
        method->show(),
        show(name_));
    auto model = model_template_.instantiate(method, context_, verbosity_);
    // If a method has an empty model, then don't add a model
    if (model) {
      models.push_back(*model);
    }
  }
  return models;
}

std::unordered_set<const MethodConstraint*>
JsonModelGeneratorItem::constraint_leaves() const {
  std::unordered_set<const MethodConstraint*> flattened_constraints;
  std::vector<const MethodConstraint*> constraints = constraint_->children();
  while (!constraints.empty()) {
    const MethodConstraint* constraint = constraints.back();
    constraints.pop_back();
    if (constraint->has_children()) {
      const std::vector<const MethodConstraint*> children =
          constraint->children();
      for (const auto* child : children) {
        constraints.push_back(child);
      }
    } else {
      flattened_constraints.insert(constraint);
    }
  }
  return flattened_constraints;
}

MethodHashedSet JsonModelGeneratorItem::may_satisfy(
    const MethodMappings& method_mappings) const {
  return constraint_->may_satisfy(method_mappings);
}

JsonFieldModelGeneratorItem::JsonFieldModelGeneratorItem(
    const ModelGeneratorName* name,
    Context& context,
    std::unique_ptr<AllOfFieldConstraint> constraint,
    FieldModelTemplate field_model_template,
    int verbosity)
    : FieldVisitorModelGenerator(name, context),
      constraint_(std::move(constraint)),
      field_model_template_(std::move(field_model_template)),
      verbosity_(verbosity) {}

std::vector<FieldModel> JsonFieldModelGeneratorItem::visit_field(
    const Field* field) const {
  std::vector<FieldModel> field_models;
  if (constraint_->satisfy(field)) {
    LOG(verbosity_,
        "Field `{}` satisfies all constraints in json model generator {}",
        field->show(),
        show(name_));
    auto field_model = field_model_template_.instantiate(field);
    // If a field has an empty model, then don't add it
    if (field_model) {
      field_models.push_back(*field_model);
    }
  }
  return field_models;
}

JsonModelGenerator::JsonModelGenerator(
    const ModelGeneratorName* name,
    Context& context,
    const std::filesystem::path& json_configuration_file,
    const Json::Value& value)
    : ModelGenerator(name, context),
      json_configuration_file_(json_configuration_file) {
  JsonValidation::check_unexpected_members(value, {"model_generators"});

  int index = 0;
  for (auto model_generator :
       JsonValidation::nonempty_array(value, /* field */ "model_generators")) {
    JsonValidation::check_unexpected_members(
        model_generator, {"find", "where", "model", "verbosity", "_comment"});
    const auto* item_name = context.model_generator_name_factory->create(
        name->identifier(), index++);

    int verbosity = model_generator.isMember("verbosity")
        ? JsonValidation::integer(model_generator, "verbosity")
        : 5; // default verbosity

    std::string find_name = JsonValidation::string(model_generator, "find");
    if (find_name == "methods") {
      std::vector<std::unique_ptr<MethodConstraint>> model_constraints;
      for (auto constraint : JsonValidation::null_or_array(
               model_generator, /* field */ "where")) {
        model_constraints.push_back(
            MethodConstraint::from_json(constraint, context));
      }
      auto model_template = ModelTemplate::from_json(
          JsonValidation::object(model_generator, /* field */ "model"),
          context);
      model_template.add_model_generator(item_name);
      items_.push_back(JsonModelGeneratorItem(
          item_name,
          context,
          std::make_unique<AllOfMethodConstraint>(std::move(model_constraints)),
          std::move(model_template),
          verbosity));
    } else if (find_name == "fields") {
      std::vector<std::unique_ptr<FieldConstraint>> field_model_constraints;
      for (auto constraint : JsonValidation::null_or_array(
               model_generator, /* field */ "where")) {
        field_model_constraints.push_back(
            FieldConstraint::from_json(constraint));
      }
      auto field_model_template = FieldModelTemplate::from_json(
          JsonValidation::object(model_generator, /* field */ "model"),
          context);
      field_model_template.add_model_generator(item_name);
      field_items_.push_back(JsonFieldModelGeneratorItem(
          item_name,
          context,
          std::make_unique<AllOfFieldConstraint>(
              std::move(field_model_constraints)),
          std::move(field_model_template),
          verbosity));
    } else {
      auto error = fmt::format("Models for `{}` are not supported.", find_name);
      ERROR(1, error);
      EventLogger::log_event(
          "model_generator_error", error, /* verbosity_level */ 1);
    }
  }
}

JsonModelGenerator JsonModelGenerator::from_file(
    const std::string& name,
    Context& context,
    const std::filesystem::path& json_configuration_file) {
  return JsonModelGenerator::from_json(
      context.model_generator_name_factory->create(name),
      context,
      json_configuration_file,
      JsonReader::parse_json_file(json_configuration_file));
}

JsonModelGenerator JsonModelGenerator::from_json(
    const std::string& name,
    Context& context,
    const std::filesystem::path& json_configuration_file,
    const Json::Value& json) {
  return JsonModelGenerator::from_json(
      context.model_generator_name_factory->create(name),
      context,
      json_configuration_file,
      json);
}

JsonModelGenerator JsonModelGenerator::from_json(
    const ModelGeneratorName* name,
    Context& context,
    const std::filesystem::path& json_configuration_file,
    const Json::Value& json) {
  return JsonModelGenerator(name, context, json_configuration_file, json);
}

std::vector<Model> JsonModelGenerator::emit_method_models(
    const Methods& methods) {
  std::vector<Model> models;
  for (auto& item : items_) {
    std::vector<Model> method_models = item.emit_method_models(methods);
    models.insert(
        models.end(),
        std::make_move_iterator(method_models.begin()),
        std::make_move_iterator(method_models.end()));
  }
  return models;
}

std::vector<Model> JsonModelGenerator::emit_method_models_optimized(
    const Methods& methods,
    const MethodMappings& method_mappings) {
  std::vector<Model> models;
  for (auto& item : items_) {
    MethodHashedSet filtered_methods = item.may_satisfy(method_mappings);

    if (filtered_methods.is_bottom()) {
      LOG(4, "Model generator `{}` emitted 0 models.", show(item.name()));
      EventLogger::log_event(
          "model_generator_match",
          show(item.name()),
          0,
          /* verbosity_level */ 3);
      continue;
    }

    std::vector<Model> method_models;
    if (filtered_methods.is_top()) {
      method_models = item.emit_method_models(methods);
    } else {
      method_models = item.emit_method_models_filtered(filtered_methods);
    }

    LOG(4,
        "Model generator `{}` emitted {} models.",
        show(item.name()),
        method_models.size());
    EventLogger::log_event(
        "model_generator_match",
        show(item.name()),
        method_models.size(),
        /* verbosity_level */ 3);

    models.insert(
        models.end(),
        std::make_move_iterator(method_models.begin()),
        std::make_move_iterator(method_models.end()));
  }
  return models;
}

std::vector<FieldModel> JsonModelGenerator::emit_field_models(
    const Fields& fields) {
  std::vector<FieldModel> models;
  for (auto& item : field_items_) {
    std::vector<FieldModel> field_models = item.emit_field_models(fields);

    LOG(4,
        "Field model generator `{}` emitted {} models.",
        show(item.name()),
        field_models.size());
    EventLogger::log_event(
        "model_generator_match",
        show(item.name()),
        field_models.size(),
        /* verbosity_level */ 3);

    models.insert(
        models.end(),
        std::make_move_iterator(field_models.begin()),
        std::make_move_iterator(field_models.end()));
  }
  return models;
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/EventLogger.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/model-generator/JsonModelGenerator.h>

namespace marianatrench {

JsonModelGeneratorItem::JsonModelGeneratorItem(
    const std::string& name,
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
        name_);
    auto model = model_template_.instantiate(context_, method);
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
    const std::string& name,
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
        name_);
    auto field_model = field_model_template_.instantiate(field);
    // If a field has an empty model, then don't add it
    if (field_model) {
      field_models.push_back(*field_model);
    }
  }
  return field_models;
}

JsonModelGenerator::JsonModelGenerator(
    const std::string& name,
    Context& context,
    const boost::filesystem::path& json_configuration_file)
    : ModelGenerator(name, context),
      json_configuration_file_(json_configuration_file) {
  const Json::Value& value =
      JsonValidation::parse_json_file(json_configuration_file);
  JsonValidation::validate_object(value);

  for (auto model_generator :
       JsonValidation::nonempty_array(value, /* field */ "model_generators")) {
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
      items_.push_back(JsonModelGeneratorItem(
          name,
          context,
          std::make_unique<AllOfMethodConstraint>(std::move(model_constraints)),
          ModelTemplate::from_json(
              JsonValidation::object(model_generator, /* field */ "model"),
              context),
          verbosity));
    } else if (find_name == "fields") {
      std::vector<std::unique_ptr<FieldConstraint>> field_model_constraints;
      for (auto constraint : JsonValidation::null_or_array(
               model_generator, /* field */ "where")) {
        field_model_constraints.push_back(
            FieldConstraint::from_json(constraint));
      }
      field_items_.push_back(JsonFieldModelGeneratorItem(
          name,
          context,
          std::make_unique<AllOfFieldConstraint>(
              std::move(field_model_constraints)),
          FieldModelTemplate::from_json(
              JsonValidation::object(model_generator, /* field */ "model"),
              context),
          verbosity));
    } else {
      auto error = fmt::format("Models for `{}` are not supported.", find_name);
      ERROR(1, error);
      EventLogger::log_event("model_generator_error", error);
    }
  }
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
  for (size_t i = 0; i < items_.size(); ++i) {
    auto& item = items_[i];
    MethodHashedSet filtered_methods = item.may_satisfy(method_mappings);
    if (filtered_methods.is_bottom()) {
      continue;
    }
    std::vector<Model> method_models;
    if (filtered_methods.is_top()) {
      method_models = item.emit_method_models(methods);
    } else {
      method_models = item.emit_method_models_filtered(filtered_methods);
    }

    EventLogger::log_event(
        "model_generator_match",
        fmt::format("{}:{}", this->name(), i),
        method_models.size());

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
    models.insert(
        models.end(),
        std::make_move_iterator(field_models.begin()),
        std::make_move_iterator(field_models.end()));
  }
  return models;
}

} // namespace marianatrench

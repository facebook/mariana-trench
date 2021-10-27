/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

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
       JsonValidation::null_or_array(value, /* field */ "model_generators")) {
    std::vector<std::unique_ptr<MethodConstraint>> model_constraints;
    int verbosity = model_generator.isMember("verbosity")
        ? JsonValidation::integer(model_generator, "verbosity")
        : 5; // default verbosity

    std::string find_name = JsonValidation::string(model_generator, "find");
    if (find_name == "methods") {
      for (auto constraint : JsonValidation::null_or_array(
               model_generator, /* field */ "where")) {
        model_constraints.push_back(
            MethodConstraint::from_json(constraint, context));
      }
    } else {
      ERROR(1, "Models for `{}` are not supported.", find_name);
    }
    items_.push_back(JsonModelGeneratorItem(
        name,
        context,
        std::make_unique<AllOfMethodConstraint>(std::move(model_constraints)),
        ModelTemplate::from_json(
            JsonValidation::object(model_generator, /* field */ "model"),
            context),
        verbosity));
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
  for (auto& item : items_) {
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
    models.insert(
        models.end(),
        std::make_move_iterator(method_models.begin()),
        std::make_move_iterator(method_models.end()));
  }
  return models;
}

} // namespace marianatrench

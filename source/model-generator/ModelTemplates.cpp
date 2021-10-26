/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/model-generator/MethodConstraints.h>
#include <mariana-trench/model-generator/ModelTemplates.h>
#include <mariana-trench/model-generator/TypeConstraints.h>

namespace marianatrench {

TemplateVariableMapping::TemplateVariableMapping() {}

void TemplateVariableMapping::insert(
    const std::string& name,
    ParameterPosition index) {
  map_.insert_or_assign(name, index);
}

std::optional<ParameterPosition> TemplateVariableMapping::at(
    const std::string& name) const {
  try {
    return map_.at(name);
  } catch (const std::out_of_range& exception) {
    return std::nullopt;
  }
}

ParameterPositionTemplate::ParameterPositionTemplate(
    ParameterPosition parameter_position)
    : parameter_position_(parameter_position) {}

ParameterPositionTemplate::ParameterPositionTemplate(
    std::string parameter_position)
    : parameter_position_(std::move(parameter_position)) {}

std::string ParameterPositionTemplate::to_string() const {
  if (std::holds_alternative<ParameterPosition>(parameter_position_)) {
    return std::to_string(std::get<ParameterPosition>(parameter_position_));
  } else if (std::holds_alternative<std::string>(parameter_position_)) {
    return std::get<std::string>(parameter_position_);
  } else {
    mt_unreachable();
  }
}

ParameterPosition ParameterPositionTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions) const {
  if (std::holds_alternative<ParameterPosition>(parameter_position_)) {
    return std::get<ParameterPosition>(parameter_position_);
  } else if (std::holds_alternative<std::string>(parameter_position_)) {
    auto result =
        parameter_positions.at(std::get<std::string>(parameter_position_));
    if (result) {
      return *result;
    } else {
      throw JsonValidationError(
          Json::Value(std::get<std::string>(parameter_position_)),
          /* field */ "parameter_position",
          /* expected */ "a variable name that is defined in \"variable\"");
    }
  } else {
    mt_unreachable();
  }
}

RootTemplate::RootTemplate(
    Root::Kind kind,
    std::optional<ParameterPositionTemplate> parameter_position)
    : kind_(kind), parameter_position_(parameter_position) {}

bool RootTemplate::is_argument() const {
  return kind_ == Root::Kind::Argument;
}

std::string RootTemplate::to_string() const {
  return is_argument()
      ? fmt::format("Argument({})", parameter_position_->to_string())
      : "Return";
}

Root RootTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions) const {
  switch (kind_) {
    case Root::Kind::Return: {
      return Root(Root::Kind::Return);
    }
    case Root::Kind::Argument: {
      mt_assert(parameter_position_);
      return Root(
          Root::Kind::Argument,
          parameter_position_->instantiate(parameter_positions));
    }
    default: {
      mt_unreachable();
    }
  }
}

AccessPathTemplate::AccessPathTemplate(RootTemplate root, Path path)
    : root_(root), path_(std::move(path)) {}

AccessPathTemplate AccessPathTemplate::from_json(const Json::Value& value) {
  auto elements = AccessPath::split_path(value);

  if (elements.empty()) {
    throw JsonValidationError(
        value, /* field */ std::nullopt, "non-empty string for access path");
  }

  // Parse the root.
  auto root = RootTemplate(Root::Kind::Return);
  const auto& root_string = elements.front();

  if (boost::starts_with(root_string, "Argument(") &&
      boost::ends_with(root_string, ")") && root_string.size() >= 11) {
    auto parameter_string = root_string.substr(9, root_string.size() - 10);
    auto parameter = parse_parameter_position(parameter_string);
    if (parameter) {
      root = RootTemplate(
          Root::Kind::Argument, ParameterPositionTemplate(*parameter));
    } else {
      root = RootTemplate(
          Root::Kind::Argument, ParameterPositionTemplate(parameter_string));
    }
  } else if (root_string != "Return") {
    throw JsonValidationError(
        value,
        /* field */ std::nullopt,
        fmt::format(
            "valid access path root (`Return` or `Argument(...)`), got `{}`",
            root_string));
  }

  Path path;
  for (auto iterator = std::next(elements.begin()), end = elements.end();
       iterator != end;
       ++iterator) {
    path.append(DexString::make_string(*iterator));
  }

  return AccessPathTemplate(root, path);
}

Json::Value AccessPathTemplate::to_json() const {
  std::string value;

  value.append(root_.to_string());
  for (auto* field : path_) {
    value.append(".");
    value.append(show(field));
  }

  return value;
}

AccessPath AccessPathTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions) const {
  return AccessPath(root_.instantiate(parameter_positions), path_);
}

PropagationTemplate::PropagationTemplate(
    AccessPathTemplate input,
    AccessPathTemplate output,
    FeatureMayAlwaysSet inferred_features,
    FeatureSet user_features)
    : input_(std::move(input)),
      output_(std::move(output)),
      inferred_features_(std::move(inferred_features)),
      user_features_(std::move(user_features)) {}

PropagationTemplate PropagationTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::string(value, /* field */ "input");
  auto input = AccessPathTemplate::from_json(value["input"]);

  if (!input.root().is_argument()) {
    throw JsonValidationError(
        value, /* field */ "input", "an access path to an argument");
  }

  JsonValidation::string(value, /* field */ "output");
  auto output = AccessPathTemplate::from_json(value["output"]);

  auto inferred_features = FeatureMayAlwaysSet::from_json(value, context);
  auto user_features = FeatureSet::from_json(value["features"], context);

  return PropagationTemplate(input, output, inferred_features, user_features);
}

void PropagationTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_propagation(
      Propagation(
          input_.instantiate(parameter_positions),
          inferred_features_,
          user_features_),
      output_.instantiate(parameter_positions));
}

SinkTemplate::SinkTemplate(Frame sink, AccessPathTemplate port)
    : sink_(std::move(sink)), port_(std::move(port)) {}

SinkTemplate SinkTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::string(value, /* field */ "port");
  return SinkTemplate(
      Frame::from_json(value, context),
      AccessPathTemplate::from_json(value["port"]));
}

void SinkTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_sink(port_.instantiate(parameter_positions), sink_);
}

ParameterSourceTemplate::ParameterSourceTemplate(
    Frame source,
    AccessPathTemplate port)
    : source_(std::move(source)), port_(std::move(port)) {}

ParameterSourceTemplate ParameterSourceTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::string(value, /* field */ "port");
  return ParameterSourceTemplate(
      Frame::from_json(value, context),
      AccessPathTemplate::from_json(value["port"]));
}

void ParameterSourceTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_parameter_source(port_.instantiate(parameter_positions), source_);
}

GenerationTemplate::GenerationTemplate(Frame source, AccessPathTemplate port)
    : source_(std::move(source)), port_(std::move(port)) {}

GenerationTemplate GenerationTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::string(value, /* field */ "port");
  return GenerationTemplate(
      Frame::from_json(value, context),
      AccessPathTemplate::from_json(value["port"]));
}

void GenerationTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_generation(port_.instantiate(parameter_positions), source_);
}

SourceTemplate::SourceTemplate(Frame source, AccessPathTemplate port)
    : source_(std::move(source)), port_(std::move(port)) {}

SourceTemplate SourceTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::string(value, /* field */ "port");
  return SourceTemplate(
      Frame::from_json(value, context),
      AccessPathTemplate::from_json(value["port"]));
}

void SourceTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  if (port_.root().is_argument()) {
    model.add_parameter_source(port_.instantiate(parameter_positions), source_);
  } else {
    model.add_generation(port_.instantiate(parameter_positions), source_);
  }
}

AttachToSourcesTemplate::AttachToSourcesTemplate(
    FeatureSet features,
    RootTemplate port)
    : features_(std::move(features)), port_(std::move(port)) {}

AttachToSourcesTemplate AttachToSourcesTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::null_or_array(value, /* field */ "features");
  auto features = FeatureSet::from_json(value["features"], context);

  JsonValidation::string(value, /* field */ "port");
  auto port = AccessPathTemplate::from_json(value["port"]);

  if (!port.path().empty()) {
    throw JsonValidationError(
        value,
        /* field */ "port",
        /* expected */ "an access path root without field");
  }

  return AttachToSourcesTemplate(features, port.root());
}

void AttachToSourcesTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_attach_to_sources(
      port_.instantiate(parameter_positions), features_);
}

AttachToSinksTemplate::AttachToSinksTemplate(
    FeatureSet features,
    RootTemplate port)
    : features_(std::move(features)), port_(std::move(port)) {}

AttachToSinksTemplate AttachToSinksTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::null_or_array(value, /* field */ "features");
  auto features = FeatureSet::from_json(value["features"], context);

  JsonValidation::string(value, /* field */ "port");
  auto port = AccessPathTemplate::from_json(value["port"]);

  if (!port.path().empty()) {
    throw JsonValidationError(
        value,
        /* field */ "port",
        /* expected */ "an access path root without field");
  }

  return AttachToSinksTemplate(features, port.root());
}

void AttachToSinksTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_attach_to_sinks(port_.instantiate(parameter_positions), features_);
}

AttachToPropagationsTemplate::AttachToPropagationsTemplate(
    FeatureSet features,
    RootTemplate port)
    : features_(std::move(features)), port_(std::move(port)) {}

AttachToPropagationsTemplate AttachToPropagationsTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::null_or_array(value, /* field */ "features");
  auto features = FeatureSet::from_json(value["features"], context);

  JsonValidation::string(value, /* field */ "port");
  auto port = AccessPathTemplate::from_json(value["port"]);

  if (!port.path().empty()) {
    throw JsonValidationError(
        value,
        /* field */ "port",
        /* expected */ "an access path root without field");
  }

  return AttachToPropagationsTemplate(features, port.root());
}

void AttachToPropagationsTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_attach_to_propagations(
      port_.instantiate(parameter_positions), features_);
}

AddFeaturesToArgumentsTemplate::AddFeaturesToArgumentsTemplate(
    FeatureSet features,
    RootTemplate port)
    : features_(std::move(features)), port_(std::move(port)) {}

AddFeaturesToArgumentsTemplate AddFeaturesToArgumentsTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::null_or_array(value, /* field */ "features");
  auto features = FeatureSet::from_json(value["features"], context);

  JsonValidation::string(value, /* field */ "port");
  auto port = AccessPathTemplate::from_json(value["port"]);

  if (!port.path().empty()) {
    throw JsonValidationError(
        value,
        /* field */ "port",
        /* expected */ "an access path root without field");
  }

  return AddFeaturesToArgumentsTemplate(features, port.root());
}

void AddFeaturesToArgumentsTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_add_features_to_arguments(
      port_.instantiate(parameter_positions), features_);
}

ForAllParameters::ForAllParameters(
    std::unique_ptr<AllOfTypeConstraint> constraints,
    std::string variable,
    std::vector<SinkTemplate> sink_templates,
    std::vector<ParameterSourceTemplate> parameter_source_templates,
    std::vector<GenerationTemplate> generation_templates,
    std::vector<SourceTemplate> source_templates,
    std::vector<PropagationTemplate> propagation_templates,
    std::vector<AttachToSourcesTemplate> attach_to_sources_templates,
    std::vector<AttachToSinksTemplate> attach_to_sinks_templates,
    std::vector<AttachToPropagationsTemplate> attach_to_propagations_templates,
    std::vector<AddFeaturesToArgumentsTemplate>
        add_features_to_arguments_templates)
    : constraints_(std::move(constraints)),
      variable_(std::move(variable)),
      sink_templates_(std::move(sink_templates)),
      parameter_source_templates_(std::move(parameter_source_templates)),
      generation_templates_(std::move(generation_templates)),
      source_templates_(std::move(source_templates)),
      propagation_templates_(std::move(propagation_templates)),
      attach_to_sources_templates_(std::move(attach_to_sources_templates)),
      attach_to_sinks_templates_(std::move(attach_to_sinks_templates)),
      attach_to_propagations_templates_(
          std::move(attach_to_propagations_templates)),
      add_features_to_arguments_templates_(
          std::move(add_features_to_arguments_templates)) {}

ForAllParameters ForAllParameters::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  std::vector<std::unique_ptr<TypeConstraint>> constraints;
  std::vector<SinkTemplate> sink_templates;
  std::vector<ParameterSourceTemplate> parameter_source_templates;
  std::vector<GenerationTemplate> generation_templates;
  std::vector<SourceTemplate> source_templates;
  std::vector<PropagationTemplate> propagation_templates;
  std::vector<AttachToSourcesTemplate> attach_to_sources_templates;
  std::vector<AttachToSinksTemplate> attach_to_sinks_templates;
  std::vector<AttachToPropagationsTemplate> attach_to_propagations_templates;
  std::vector<AddFeaturesToArgumentsTemplate>
      add_features_to_arguments_templates;

  std::string variable = JsonValidation::string(value, "variable");

  for (auto constraint :
       JsonValidation::null_or_array(value, /* field */ "where")) {
    // We assume constraints on parameters are type constraints
    constraints.push_back(TypeConstraint::from_json(constraint));
  }

  for (auto sink_value :
       JsonValidation::null_or_array(value, /* field */ "sinks")) {
    sink_templates.push_back(SinkTemplate::from_json(sink_value, context));
  }

  for (auto source_value :
       JsonValidation::null_or_array(value, /* field */ "parameter_sources")) {
    parameter_source_templates.push_back(
        ParameterSourceTemplate::from_json(source_value, context));
  }

  for (auto source_value :
       JsonValidation::null_or_array(value, /* field */ "generations")) {
    generation_templates.push_back(
        GenerationTemplate::from_json(source_value, context));
  }

  for (auto source_value :
       JsonValidation::null_or_array(value, /* field */ "sources")) {
    source_templates.push_back(
        SourceTemplate::from_json(source_value, context));
  }

  for (auto propagation_value :
       JsonValidation::null_or_array(value, /* field */ "propagation")) {
    propagation_templates.push_back(
        PropagationTemplate::from_json(propagation_value, context));
  }

  for (auto attach_to_sources_value :
       JsonValidation::null_or_array(value, /* field */ "attach_to_sources")) {
    attach_to_sources_templates.push_back(
        AttachToSourcesTemplate::from_json(attach_to_sources_value, context));
  }

  for (auto attach_to_sinks_value :
       JsonValidation::null_or_array(value, /* field */ "attach_to_sinks")) {
    attach_to_sinks_templates.push_back(
        AttachToSinksTemplate::from_json(attach_to_sinks_value, context));
  }

  for (auto attach_to_propagations_value : JsonValidation::null_or_array(
           value, /* field */ "attach_to_propagations")) {
    attach_to_propagations_templates.push_back(
        AttachToPropagationsTemplate::from_json(
            attach_to_propagations_value, context));
  }

  for (auto add_features_to_arguments_value : JsonValidation::null_or_array(
           value, /* field */ "add_features_to_arguments")) {
    add_features_to_arguments_templates.push_back(
        AddFeaturesToArgumentsTemplate::from_json(
            add_features_to_arguments_value, context));
  }

  return ForAllParameters(
      std::make_unique<AllOfTypeConstraint>(std::move(constraints)),
      variable,
      sink_templates,
      parameter_source_templates,
      generation_templates,
      source_templates,
      propagation_templates,
      attach_to_sources_templates,
      attach_to_sinks_templates,
      attach_to_propagations_templates,
      add_features_to_arguments_templates);
}

bool ForAllParameters::instantiate(Model& model, const Method* method) const {
  bool updated = false;
  ParameterPosition index = method->first_parameter_index();
  for (auto type : *method->get_proto()->get_args()) {
    if (constraints_->satisfy(type)) {
      LOG(3, "Type {} satifies constraints in for_all_parameters", show(type));
      TemplateVariableMapping variable_mapping;
      variable_mapping.insert(variable_, index);

      for (const auto& sink_template : sink_templates_) {
        sink_template.instantiate(variable_mapping, model);
        updated = true;
      }
      for (const auto& parameter_source_template :
           parameter_source_templates_) {
        parameter_source_template.instantiate(variable_mapping, model);
        updated = true;
      }
      for (const auto& generation_template : generation_templates_) {
        generation_template.instantiate(variable_mapping, model);
        updated = true;
      }
      for (const auto& source_template : source_templates_) {
        source_template.instantiate(variable_mapping, model);
        updated = true;
      }
      for (const auto& propagation_template : propagation_templates_) {
        propagation_template.instantiate(variable_mapping, model);
        updated = true;
      }
      for (const auto& attach_to_sources_template :
           attach_to_sources_templates_) {
        attach_to_sources_template.instantiate(variable_mapping, model);
        updated = true;
      }
      for (const auto& attach_to_sinks_template : attach_to_sinks_templates_) {
        attach_to_sinks_template.instantiate(variable_mapping, model);
        updated = true;
      }
      for (const auto& attach_to_propagations_template :
           attach_to_propagations_templates_) {
        attach_to_propagations_template.instantiate(variable_mapping, model);
        updated = true;
      }
      for (const auto& add_features_to_arguments_template :
           add_features_to_arguments_templates_) {
        add_features_to_arguments_template.instantiate(variable_mapping, model);
        updated = true;
      }
    }
    index++;
  }
  return updated;
}

ModelTemplate::ModelTemplate(
    const Model& model,
    std::vector<ForAllParameters> for_all_parameters)
    : model_(model), for_all_parameters_(std::move(for_all_parameters)) {
  mt_assert(model.method() == nullptr);
}

std::optional<Model> ModelTemplate::instantiate(
    Context& context,
    const Method* method) const {
  Model model = model_.instantiate(method, context);

  bool updated = false;
  for (const auto& for_all_parameters : for_all_parameters_) {
    bool result = for_all_parameters.instantiate(model, method);
    updated = updated || result;
  }

  // An instantiated model can be nonempty even when it is instantiated from
  // an empty model and no new sinks/generations/propagations/sources were
  // introduced by for_all_parameters, because it is possible that a model has
  // non-zero propagations after instantiation (because Model constructor may
  // add default propagations)
  if (!model_.empty() || updated) {
    return model;
  } else {
    LOG(3,
        "Method {} generates no new sinks/generations/propagations/sources from {} for_all_parameters constraints:\nInstantiated model: {}.\nModel template: {}.",
        method->show(),
        for_all_parameters_.size(),
        JsonValidation::to_styled_string(model.to_json()),
        JsonValidation::to_styled_string(model_.to_json()));
    return std::nullopt;
  }
}

ModelTemplate ModelTemplate::from_json(
    const Json::Value& model,
    Context& context) {
  JsonValidation::validate_object(model);

  std::vector<ForAllParameters> for_all_parameters;
  for (auto const& value :
       JsonValidation::null_or_array(model, /* field */ "for_all_parameters")) {
    for_all_parameters.push_back(ForAllParameters::from_json(value, context));
  }

  return ModelTemplate(
      Model::from_json(
          /* method */ nullptr, model, context),
      std::move(for_all_parameters));
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/constraints/MethodConstraints.h>
#include <mariana-trench/constraints/ParameterConstraints.h>
#include <mariana-trench/model-generator/ModelTemplates.h>

namespace marianatrench {

void TemplateVariableMapping::insert(
    const std::string& name,
    ParameterPosition index) {
  map_.insert_or_assign(name, index);
}

std::optional<ParameterPosition> TemplateVariableMapping::at(
    const std::string& name) const {
  try {
    return map_.at(name);
  } catch (const std::out_of_range&) {
    return std::nullopt;
  }
}

AccessPathTemplate::AccessPathTemplate(RootTemplate root, Path path)
    : root_(root), path_(std::move(path)) {}

AccessPathTemplate AccessPathTemplate::from_json(const Json::Value& value) {
  auto elements = Path::split_path(JsonValidation::string(value));

  if (elements.empty()) {
    throw JsonValidationError(
        value, /* field */ std::nullopt, "non-empty string for access path");
  }

  auto root = RootTemplate::from_json(elements.front());
  Path path;
  for (auto iterator = std::next(elements.begin()), end = elements.end();
       iterator != end;
       ++iterator) {
    path.append(PathElement::field(*iterator));
  }

  return AccessPathTemplate(std::move(root), path);
}

Json::Value AccessPathTemplate::to_json() const {
  std::string value = root_.to_string();

  for (const auto& field : path_) {
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
    FeatureSet user_features,
    const TransformList* transforms,
    CollapseDepth collapse_depth)
    : input_(std::move(input)),
      output_(std::move(output)),
      inferred_features_(std::move(inferred_features)),
      user_features_(std::move(user_features)),
      transforms_(transforms),
      collapse_depth_(collapse_depth) {}

PropagationTemplate PropagationTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);
  JsonValidation::check_unexpected_members(
      value,
      {"input",
       "output",
       "may_features",
       "always_features",
       "features",
       "transforms",
       "collapse",
       "collapse-depth"});

  JsonValidation::string(value, /* field */ "input");
  auto input = AccessPathTemplate::from_json(value["input"]);

  if (!input.root().is_argument()) {
    throw JsonValidationError(
        value, /* field */ "input", "an access path to an argument");
  }

  JsonValidation::string(value, /* field */ "output");
  auto output = AccessPathTemplate::from_json(value["output"]);

  auto inferred_features = FeatureMayAlwaysSet::from_json(
      value, context, /* check_unexpected_members */ false);
  auto user_features = FeatureSet::from_json(value["features"], context);

  const TransformList* transforms = nullptr;
  if (value.isMember("transforms")) {
    transforms = context.transforms_factory->create(
        TransformList::from_json(value["transforms"], context));
  }

  auto collapse_depth = CollapseDepth::zero();
  if (value.isMember("collapse")) {
    collapse_depth = JsonValidation::boolean(value, "collapse")
        ? CollapseDepth::zero()
        : CollapseDepth::no_collapse();
  } else if (value.isMember("collapse-depth")) {
    int collapse_depth_integer =
        JsonValidation::integer(value, "collapse-depth");
    if (collapse_depth_integer < 0) {
      throw JsonValidationError(
          value, "collapse-depth", /* expected */ "non-negative integer");
    }
    collapse_depth = CollapseDepth(collapse_depth_integer);
  }

  return PropagationTemplate(
      input,
      output,
      inferred_features,
      user_features,
      transforms,
      collapse_depth);
}

void PropagationTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model,
    Context& context) const {
  auto input_port = input_.instantiate(parameter_positions);
  auto output_port = output_.instantiate(parameter_positions);
  const PropagationKind* propagation_kind = nullptr;
  if (output_port.root().is_return()) {
    propagation_kind = context.kind_factory->local_return();
  } else if (output_port.root().is_argument()) {
    propagation_kind = context.kind_factory->local_argument(
        output_port.root().parameter_position());
  } else {
    throw JsonValidationError(
        output_port.to_json(),
        /* field */ "output",
        "an access path with a `Return` or `Argument(x)` root");
  }

  const Kind* kind = propagation_kind;
  if (transforms_ != nullptr) {
    kind = context.kind_factory->transform_kind(
        /* base_kind */ propagation_kind,
        /* local_transforms */
        transforms_,
        /* global_transforms */ nullptr);
  }

  model.add_propagation(
      PropagationConfig(
          input_port,
          kind,
          PathTreeDomain{{output_port.path(), collapse_depth_}},
          inferred_features_,
          /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
          user_features_),
      *context.heuristics);
}

PortSanitizerTemplate::PortSanitizerTemplate(
    SanitizerKind sanitizer_kind,
    RootTemplate port,
    KindSetAbstractDomain kinds)
    : sanitizer_kind_(std::move(sanitizer_kind)),
      port_(std::move(port)),
      kinds_(std::move(kinds)) {}

PortSanitizerTemplate PortSanitizerTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);
  JsonValidation::check_unexpected_members(
      value, {"kinds", "port", "sanitize"});

  SanitizerKind sanitizer_kind;
  auto sanitizer_kind_string =
      JsonValidation::string(value, /* field */ "sanitize");
  if (sanitizer_kind_string == "sources") {
    sanitizer_kind = SanitizerKind::Sources;
  } else if (sanitizer_kind_string == "sinks") {
    sanitizer_kind = SanitizerKind::Sinks;
  } else if (sanitizer_kind_string == "propagations") {
    sanitizer_kind = SanitizerKind::Propagations;
  } else {
    throw JsonValidationError(
        value,
        /* field */ "sanitizer",
        /* expected */ "`sources`, `sinks` or `propagations`");
  }

  auto port = AccessPathTemplate::from_json(value["port"]);

  auto kinds = KindSetAbstractDomain::bottom();
  if (value.isMember("kinds")) {
    kinds = KindSetAbstractDomain();
    for (const auto& kind_json :
         JsonValidation::nonempty_array(value, "kinds")) {
      kinds.add(
          SourceSinkKind::from_config_json(kind_json, context, sanitizer_kind));
    }
  } else {
    kinds = KindSetAbstractDomain::top();
  }

  return PortSanitizerTemplate(sanitizer_kind, port.root(), std::move(kinds));
}

void PortSanitizerTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  auto root = port_.instantiate(parameter_positions);
  auto sanitizer = Sanitizer(sanitizer_kind_, kinds_);

  model.add_port_sanitizers(SanitizerSet(sanitizer), root);
}

SinkTemplate::SinkTemplate(TaintConfigTemplate sink, AccessPathTemplate port)
    : sink_(std::move(sink)), port_(std::move(port)) {}

SinkTemplate SinkTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::string(value, /* field */ "port");
  return SinkTemplate(
      TaintConfigTemplate::from_json(value, context),
      AccessPathTemplate::from_json(value["port"]));
}

void SinkTemplate::instantiate(
    const Method* method,
    Context& context,
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_sink(
      port_.instantiate(parameter_positions),
      sink_.instantiate(method, context, parameter_positions),
      *context.heuristics);
}

ParameterSourceTemplate::ParameterSourceTemplate(
    TaintConfigTemplate source,
    AccessPathTemplate port)
    : source_(std::move(source)), port_(std::move(port)) {}

ParameterSourceTemplate ParameterSourceTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::string(value, /* field */ "port");
  return ParameterSourceTemplate(
      TaintConfigTemplate::from_json(value, context),
      AccessPathTemplate::from_json(value["port"]));
}

void ParameterSourceTemplate::instantiate(
    const Method* const method,
    Context& context,
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_parameter_source(
      port_.instantiate(parameter_positions),
      source_.instantiate(method, context, parameter_positions),
      *context.heuristics);
}

GenerationTemplate::GenerationTemplate(
    TaintConfigTemplate source,
    AccessPathTemplate port)
    : source_(std::move(source)), port_(std::move(port)) {}

GenerationTemplate GenerationTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::string(value, /* field */ "port");
  return GenerationTemplate(
      TaintConfigTemplate::from_json(value, context),
      AccessPathTemplate::from_json(value["port"]));
}

void GenerationTemplate::instantiate(
    const Method* method,
    Context& context,
    const TemplateVariableMapping& parameter_positions,
    Model& model) const {
  model.add_generation(
      port_.instantiate(parameter_positions),
      source_.instantiate(method, context, parameter_positions),
      *context.heuristics);
}

SourceTemplate::SourceTemplate(TaintConfig source, AccessPathTemplate port)
    : source_(std::move(source)), port_(std::move(port)) {}

SourceTemplate SourceTemplate::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::string(value, /* field */ "port");
  return SourceTemplate(
      TaintConfig::from_json(value, context),
      AccessPathTemplate::from_json(value["port"]));
}

void SourceTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions,
    Model& model,
    const Heuristics& heuristics) const {
  if (port_.root().is_argument()) {
    model.add_parameter_source(
        port_.instantiate(parameter_positions), source_, heuristics);
  } else {
    model.add_generation(
        port_.instantiate(parameter_positions), source_, heuristics);
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
  JsonValidation::check_unexpected_members(value, {"port", "features"});

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
  JsonValidation::check_unexpected_members(value, {"port", "features"});

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
  JsonValidation::check_unexpected_members(value, {"port", "features"});

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
  JsonValidation::check_unexpected_members(value, {"port", "features"});

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
    std::unique_ptr<AllOfParameterConstraint> constraints,
    std::string variable,
    std::vector<SinkTemplate> sink_templates,
    std::vector<ParameterSourceTemplate> parameter_source_templates,
    std::vector<GenerationTemplate> generation_templates,
    std::vector<SourceTemplate> source_templates,
    std::vector<PropagationTemplate> propagation_templates,
    std::vector<PortSanitizerTemplate> port_sanitizers,
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
      port_sanitizers_(std::move(port_sanitizers)),
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
  JsonValidation::check_unexpected_members(
      value,
      {"variable",
       "where",
       "sinks",
       "parameter_sources",
       "generations",
       "sources",
       "propagation",
       "sanitizers",
       "attach_to_sources",
       "attach_to_sinks",
       "attach_to_propagations",
       "add_features_to_arguments"});

  std::vector<std::unique_ptr<ParameterConstraint>> constraints;
  std::vector<SinkTemplate> sink_templates;
  std::vector<ParameterSourceTemplate> parameter_source_templates;
  std::vector<GenerationTemplate> generation_templates;
  std::vector<SourceTemplate> source_templates;
  std::vector<PropagationTemplate> propagation_templates;
  std::vector<PortSanitizerTemplate> port_sanitizers;
  std::vector<AttachToSourcesTemplate> attach_to_sources_templates;
  std::vector<AttachToSinksTemplate> attach_to_sinks_templates;
  std::vector<AttachToPropagationsTemplate> attach_to_propagations_templates;
  std::vector<AddFeaturesToArgumentsTemplate>
      add_features_to_arguments_templates;

  std::string variable = JsonValidation::string(value, "variable");

  for (auto constraint :
       JsonValidation::null_or_array(value, /* field */ "where")) {
    // We assume constraints on parameters are type constraints
    constraints.push_back(ParameterConstraint::from_json(constraint));
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

  for (auto sanitizer_value :
       JsonValidation::null_or_array(value, /* field */ "sanitizers")) {
    port_sanitizers.push_back(
        PortSanitizerTemplate::from_json(sanitizer_value, context));
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
      std::make_unique<AllOfParameterConstraint>(std::move(constraints)),
      variable,
      sink_templates,
      parameter_source_templates,
      generation_templates,
      source_templates,
      propagation_templates,
      port_sanitizers,
      attach_to_sources_templates,
      attach_to_sinks_templates,
      attach_to_propagations_templates,
      add_features_to_arguments_templates);
}

bool ForAllParameters::instantiate(
    Model& model,
    const Method* method,
    Context& context,
    int verbosity) const {
  bool updated = false;
  ParameterPosition index = method->first_parameter_index();
  for (auto type : *method->get_proto()->get_args()) {
    const auto* annotations_set = method->get_parameter_annotations(index);

    if (constraints_->satisfy(annotations_set, type)) {
      LOG(verbosity,
          "Type `{}` satifies constraints in for_all_parameters",
          show(type));
      TemplateVariableMapping variable_mapping;
      variable_mapping.insert(variable_, index);

      for (const auto& sink_template : sink_templates_) {
        sink_template.instantiate(method, context, variable_mapping, model);
        updated = true;
      }
      for (const auto& parameter_source_template :
           parameter_source_templates_) {
        parameter_source_template.instantiate(
            method, context, variable_mapping, model);
        updated = true;
      }
      for (const auto& generation_template : generation_templates_) {
        generation_template.instantiate(
            method, context, variable_mapping, model);
        updated = true;
      }
      for (const auto& source_template : source_templates_) {
        source_template.instantiate(
            variable_mapping, model, *context.heuristics);
        updated = true;
      }
      for (const auto& propagation_template : propagation_templates_) {
        propagation_template.instantiate(variable_mapping, model, context);
        updated = true;
      }
      for (const auto& port_sanitizer : port_sanitizers_) {
        port_sanitizer.instantiate(variable_mapping, model);
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
    std::vector<ForAllParameters> for_all_parameters,
    std::vector<std::pair<AccessPath, TaintConfigTemplate>> generations,
    std::vector<std::pair<AccessPath, TaintConfigTemplate>> parameter_sources,
    std::vector<std::pair<AccessPath, TaintConfigTemplate>> sinks)
    : model_(model),
      for_all_parameters_(std::move(for_all_parameters)),
      generations_(std::move(generations)),
      parameter_sources_(std::move(parameter_sources)),
      sinks_(std::move(sinks)) {
  mt_assert(model.method() == nullptr);
}

void ModelTemplate::add_model_generator(
    const ModelGeneratorName* model_generator) {
  model_.add_model_generator(model_generator);
}

std::optional<Model> ModelTemplate::instantiate(
    const Method* method,
    Context& context,
    int verbosity) const {
  Model model = model_.instantiate(method, context);

  bool updated = false;
  for (const auto& for_all_parameters : for_all_parameters_) {
    bool result =
        for_all_parameters.instantiate(model, method, context, verbosity);
    updated = updated || result;
  }

  for (const auto& [port, taint_config_template] : generations_) {
    model.add_generation(
        port,
        taint_config_template.instantiate(method, context),
        *context.heuristics);
  }
  for (const auto& [port, taint_config_template] : parameter_sources_) {
    model.add_parameter_source(
        port,
        taint_config_template.instantiate(method, context),
        *context.heuristics);
  }
  for (const auto& [port, taint_config_template] : sinks_) {
    model.add_sink(
        port,
        taint_config_template.instantiate(method, context),
        *context.heuristics);
  }

  // An instantiated model can be nonempty even when it is instantiated from
  // an empty model and no new sinks/generations/propagations/sources were
  // introduced by for_all_parameters, because it is possible that a model has
  // non-zero propagations after instantiation (because Model constructor may
  // add default propagations)
  if (!model_.empty() || updated) {
    return model;
  } else {
    LOG(verbosity,
        "Method `{}` generates no new sinks/generations/propagations/sources from {} for_all_parameters constraints:\nInstantiated model: {}.\nModel template: {}.",
        method->show(),
        for_all_parameters_.size(),
        JsonWriter::to_styled_string(
            model.to_json(context.options->export_origins_mode())),
        JsonWriter::to_styled_string(
            model_.to_json(context.options->export_origins_mode())));
    return std::nullopt;
  }
}

/**
 * Helper to convert TaintConfigTemplate JSON objects in bulk to actual
 * TaintConfigTemplate instances using TaintConfigTemplate::from_json.
 * Explicitly demands move semantics because it is only used in one location
 * where that is the intent.
 */
static std::vector<std::pair<AccessPath, TaintConfigTemplate>>
taint_config_template_from_json(
    Context& context,
    std::vector<std::pair<AccessPath, Json::Value>>&& json) {
  std::vector<std::pair<AccessPath, TaintConfigTemplate>>
      taint_config_templates;
  std::transform(
      std::make_move_iterator(json.begin()),
      std::make_move_iterator(json.end()),
      std::back_inserter(taint_config_templates),
      [&context](std::pair<AccessPath, Json::Value>&& port_and_json) {
        return std::make_pair(
            std::move(port_and_json.first),
            TaintConfigTemplate::from_json(port_and_json.second, context));
      });
  return taint_config_templates;
}

ModelTemplate ModelTemplate::from_json(
    const Json::Value& model,
    Context& context) {
  JsonValidation::validate_object(model);
  JsonValidation::check_unexpected_members(
      model,
      {"for_all_parameters",
       "modes",
       "freeze",
       "config_overrides",
       "generations",
       "parameter_sources",
       "sources",
       "sinks",
       "effect_sources",
       "effect_sinks",
       "propagation",
       "sanitizers",
       "attach_to_sources",
       "attach_to_sinks",
       "attach_to_propagations",
       "add_features_to_arguments",
       "inline_as_getter",
       "issues"});

  std::vector<ForAllParameters> for_all_parameters;
  for (auto const& value :
       JsonValidation::null_or_array(model, /* field */ "for_all_parameters")) {
    for_all_parameters.emplace_back(
        ForAllParameters::from_json(value, context));
  }

  std::vector<std::pair<AccessPath, Json::Value>> generation_templates;
  std::vector<std::pair<AccessPath, Json::Value>> parameter_source_templates;
  std::vector<std::pair<AccessPath, Json::Value>> sink_templates;
  Model::read_taint_configs_from_json(
      model,
      generation_templates,
      parameter_source_templates,
      sink_templates,
      TaintConfigTemplate::is_template);

  return ModelTemplate(
      Model::from_config_json(
          /* method */ nullptr,
          model,
          context,
          /* check_unexpected_members */ false),
      std::move(for_all_parameters),
      taint_config_template_from_json(context, std::move(generation_templates)),
      taint_config_template_from_json(
          context, std::move(parameter_source_templates)),
      taint_config_template_from_json(context, std::move(sink_templates)));
}

} // namespace marianatrench

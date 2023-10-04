/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Access.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/TaintConfig.h>
#include <mariana-trench/TransformList.h>
#include <mariana-trench/model-generator/ModelGenerator.h>
#include <mariana-trench/model-generator/ParameterPositionTemplate.h>
#include <mariana-trench/model-generator/RootTemplate.h>

namespace marianatrench {
class TemplateVariableMapping final {
 public:
  TemplateVariableMapping() = default;

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(TemplateVariableMapping)

  void insert(const std::string& name, ParameterPosition index);
  std::optional<ParameterPosition> at(const std::string& name) const;

 private:
  std::unordered_map<std::string, ParameterPosition> map_;
};

class AccessPathTemplate final {
 public:
  explicit AccessPathTemplate(RootTemplate root, Path path = {});

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(AccessPathTemplate)

  const RootTemplate& root() const {
    return root_;
  }

  const Path& path() const {
    return path_;
  }

  static AccessPathTemplate from_json(const Json::Value& value);
  Json::Value to_json() const;
  AccessPath instantiate(
      const TemplateVariableMapping& parameter_positions) const;

 private:
  RootTemplate root_;
  Path path_;
};

class PropagationTemplate final {
 public:
  explicit PropagationTemplate(
      AccessPathTemplate input,
      AccessPathTemplate output,
      FeatureMayAlwaysSet inferred_features,
      FeatureSet user_features,
      const TransformList* transforms,
      CollapseDepth collapse_depth);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(PropagationTemplate)

  static PropagationTemplate from_json(
      const Json::Value& value,
      Context& context);
  void instantiate(
      const TemplateVariableMapping& parameter_positions,
      Model& model,
      Context& context) const;

 private:
  AccessPathTemplate input_;
  AccessPathTemplate output_;
  FeatureMayAlwaysSet inferred_features_;
  FeatureSet user_features_;
  const TransformList* transforms_;
  CollapseDepth collapse_depth_;
};

class PortSanitizerTemplate final {
 public:
  explicit PortSanitizerTemplate(
      SanitizerKind sanitizer_kind,
      RootTemplate port);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(PortSanitizerTemplate)

  static PortSanitizerTemplate from_json(const Json::Value& value);
  void instantiate(
      const TemplateVariableMapping& parameter_positions,
      Model& model) const;

 private:
  SanitizerKind sanitizer_kind_;
  RootTemplate port_;
};

class SinkTemplate final {
 public:
  explicit SinkTemplate(TaintConfig sink, AccessPathTemplate port);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(SinkTemplate)

  static SinkTemplate from_json(const Json::Value& value, Context& context);
  void instantiate(
      const TemplateVariableMapping& parameter_positions,
      Model& model) const;

 private:
  TaintConfig sink_;
  AccessPathTemplate port_;
};

class ParameterSourceTemplate final {
 public:
  explicit ParameterSourceTemplate(TaintConfig source, AccessPathTemplate port);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ParameterSourceTemplate)

  static ParameterSourceTemplate from_json(
      const Json::Value& value,
      Context& context);
  void instantiate(
      const TemplateVariableMapping& parameter_positions,
      Model& model) const;

 private:
  TaintConfig source_;
  AccessPathTemplate port_;
};

class GenerationTemplate final {
 public:
  explicit GenerationTemplate(TaintConfig source, AccessPathTemplate port);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(GenerationTemplate)

  static GenerationTemplate from_json(
      const Json::Value& value,
      Context& context);
  void instantiate(
      const TemplateVariableMapping& parameter_positions,
      Model& model) const;

 private:
  TaintConfig source_;
  AccessPathTemplate port_;
};

class SourceTemplate final {
 public:
  explicit SourceTemplate(TaintConfig source, AccessPathTemplate port);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(SourceTemplate)

  static SourceTemplate from_json(const Json::Value& value, Context& context);
  void instantiate(
      const TemplateVariableMapping& parameter_positions,
      Model& model) const;

 private:
  TaintConfig source_;
  AccessPathTemplate port_;
};

class AttachToSourcesTemplate final {
 public:
  explicit AttachToSourcesTemplate(FeatureSet features, RootTemplate port);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(AttachToSourcesTemplate)

  static AttachToSourcesTemplate from_json(
      const Json::Value& value,
      Context& context);
  void instantiate(
      const TemplateVariableMapping& parameter_positions,
      Model& model) const;

 private:
  FeatureSet features_;
  RootTemplate port_;
};

class AttachToSinksTemplate final {
 public:
  explicit AttachToSinksTemplate(FeatureSet features, RootTemplate port);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(AttachToSinksTemplate)

  static AttachToSinksTemplate from_json(
      const Json::Value& value,
      Context& context);
  void instantiate(
      const TemplateVariableMapping& parameter_positions,
      Model& model) const;

 private:
  FeatureSet features_;
  RootTemplate port_;
};

class AttachToPropagationsTemplate final {
 public:
  explicit AttachToPropagationsTemplate(FeatureSet features, RootTemplate port);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(
      AttachToPropagationsTemplate)

  static AttachToPropagationsTemplate from_json(
      const Json::Value& value,
      Context& context);
  void instantiate(
      const TemplateVariableMapping& parameter_positions,
      Model& model) const;

 private:
  FeatureSet features_;
  RootTemplate port_;
};

class AddFeaturesToArgumentsTemplate final {
 public:
  explicit AddFeaturesToArgumentsTemplate(
      FeatureSet features,
      RootTemplate port);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(
      AddFeaturesToArgumentsTemplate)

  static AddFeaturesToArgumentsTemplate from_json(
      const Json::Value& value,
      Context& context);
  void instantiate(
      const TemplateVariableMapping& parameter_positions,
      Model& model) const;

 private:
  FeatureSet features_;
  RootTemplate port_;
};

class ForAllParameters final {
 public:
  explicit ForAllParameters(
      std::unique_ptr<class AllOfParameterConstraint> constraints,
      std::string variable,
      std::vector<SinkTemplate> sink_templates = {},
      std::vector<ParameterSourceTemplate> parameter_source_templates = {},
      std::vector<GenerationTemplate> generation_templates = {},
      std::vector<SourceTemplate> source_templates = {},
      std::vector<PropagationTemplate> propagation_templates = {},
      std::vector<PortSanitizerTemplate> port_sanitizers = {},
      std::vector<AttachToSourcesTemplate> attach_to_sources_templates = {},
      std::vector<AttachToSinksTemplate> attach_to_sinks_templates = {},
      std::vector<AttachToPropagationsTemplate>
          attach_to_propagations_templates = {},
      std::vector<AddFeaturesToArgumentsTemplate>
          add_features_to_arguments_templates = {});

  ForAllParameters(const ForAllParameters& other) = delete;
  ForAllParameters(ForAllParameters&& other) = default;
  ForAllParameters& operator=(const ForAllParameters& other) = delete;
  ForAllParameters& operator=(ForAllParameters&& other) = delete;
  ~ForAllParameters() = default;

  static ForAllParameters from_json(const Json::Value&, Context& context);

  /* Update a model with new sinks/generations/... when the model is/will be
   * instantiated with the method. Return true if the model was updated. */
  bool instantiate(
      Model& model,
      const Method* method,
      Context& context,
      int verbosity) const;

 private:
  std::unique_ptr<AllOfParameterConstraint> constraints_;
  std::string variable_;
  std::vector<SinkTemplate> sink_templates_;
  std::vector<ParameterSourceTemplate> parameter_source_templates_;
  std::vector<GenerationTemplate> generation_templates_;
  std::vector<SourceTemplate> source_templates_;
  std::vector<PropagationTemplate> propagation_templates_;
  std::vector<PortSanitizerTemplate> port_sanitizers_;
  std::vector<AttachToSourcesTemplate> attach_to_sources_templates_;
  std::vector<AttachToSinksTemplate> attach_to_sinks_templates_;
  std::vector<AttachToPropagationsTemplate> attach_to_propagations_templates_;
  std::vector<AddFeaturesToArgumentsTemplate>
      add_features_to_arguments_templates_;
};

class ModelTemplate final {
 public:
  /* The given `model` must not be associated with a method. */
  ModelTemplate(
      const Model& model,
      std::vector<ForAllParameters> for_all_parameters);

  ModelTemplate(const ModelTemplate& other) = delete;
  ModelTemplate(ModelTemplate&& other) = default;
  ModelTemplate& operator=(const ModelTemplate& other) = delete;
  ModelTemplate& operator=(ModelTemplate&& other) = delete;
  ~ModelTemplate() = default;

  void add_model_generator(const ModelGeneratorName* model_generator);

  /* Create a model with information that is associated with a method (e.g. new
   * sinks/generations/...). */
  std::optional<Model>
  instantiate(const Method* method, Context& context, int verbosity) const;
  static ModelTemplate from_json(
      const Json::Value& model_generator,
      Context& context);

 private:
  Model model_;
  std::vector<ForAllParameters> for_all_parameters_;
};
} // namespace marianatrench

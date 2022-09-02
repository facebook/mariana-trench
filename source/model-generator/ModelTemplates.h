/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Access.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/TaintConfig.h>
#include <mariana-trench/model-generator/ModelGenerator.h>

namespace marianatrench {
class TemplateVariableMapping final {
 public:
  TemplateVariableMapping();
  TemplateVariableMapping(const TemplateVariableMapping&) = default;
  TemplateVariableMapping(TemplateVariableMapping&&) = default;
  TemplateVariableMapping& operator=(const TemplateVariableMapping&) = default;
  TemplateVariableMapping& operator=(TemplateVariableMapping&&) = default;
  ~TemplateVariableMapping() = default;

  void insert(const std::string& name, ParameterPosition index);
  std::optional<ParameterPosition> at(const std::string& name) const;

 private:
  std::unordered_map<std::string, ParameterPosition> map_;
};

/* Store either an integer typed parameter position, or a string typed parameter
 * position (which is its name and can be instantiated when given a mapping from
 * variable names to variable indices) */
class ParameterPositionTemplate final {
 public:
  explicit ParameterPositionTemplate(ParameterPosition parameter_position);
  explicit ParameterPositionTemplate(std::string parameter_position);
  ParameterPositionTemplate(const ParameterPositionTemplate&) = default;
  ParameterPositionTemplate(ParameterPositionTemplate&&) = default;
  ParameterPositionTemplate& operator=(const ParameterPositionTemplate&) =
      default;
  ParameterPositionTemplate& operator=(ParameterPositionTemplate&&) = default;
  ~ParameterPositionTemplate() = default;

  ParameterPosition instantiate(
      const TemplateVariableMapping& parameter_positions) const;
  std::string to_string() const;

 private:
  std::variant<ParameterPosition, std::string> parameter_position_;
};

class RootTemplate final {
 public:
  explicit RootTemplate(
      Root::Kind kind,
      std::optional<ParameterPositionTemplate> parameter_position =
          std::nullopt);
  RootTemplate(const RootTemplate&) = default;
  RootTemplate(RootTemplate&&) = default;
  RootTemplate& operator=(const RootTemplate&) = default;
  RootTemplate& operator=(RootTemplate&&) = default;
  ~RootTemplate() = default;

  bool is_argument() const;

  Root instantiate(const TemplateVariableMapping& parameter_positions) const;
  std::string to_string() const;

 private:
  Root::Kind kind_;
  std::optional<ParameterPositionTemplate> parameter_position_;
};

class AccessPathTemplate final {
 public:
  explicit AccessPathTemplate(RootTemplate root, Path path = {});
  AccessPathTemplate(const AccessPathTemplate&) = default;
  AccessPathTemplate(AccessPathTemplate&&) = default;
  AccessPathTemplate& operator=(const AccessPathTemplate&) = delete;
  AccessPathTemplate& operator=(AccessPathTemplate&&) = delete;
  ~AccessPathTemplate() = default;

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
      FeatureSet user_features);
  PropagationTemplate(const PropagationTemplate&) = default;
  PropagationTemplate(PropagationTemplate&&) = default;
  PropagationTemplate& operator=(const PropagationTemplate&) = delete;
  PropagationTemplate& operator=(PropagationTemplate&&) = delete;
  ~PropagationTemplate() = default;

  static PropagationTemplate from_json(
      const Json::Value& value,
      Context& context);
  void instantiate(
      const TemplateVariableMapping& parameter_positions,
      Model& model) const;

 private:
  AccessPathTemplate input_;
  AccessPathTemplate output_;
  FeatureMayAlwaysSet inferred_features_;
  FeatureSet user_features_;
};

class SinkTemplate final {
 public:
  explicit SinkTemplate(TaintConfig sink, AccessPathTemplate port);
  SinkTemplate(const SinkTemplate&) = default;
  SinkTemplate(SinkTemplate&&) = default;
  SinkTemplate& operator=(const SinkTemplate&) = delete;
  SinkTemplate& operator=(SinkTemplate&&) = delete;
  ~SinkTemplate() = default;

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
  ParameterSourceTemplate(const ParameterSourceTemplate&) = default;
  ParameterSourceTemplate(ParameterSourceTemplate&&) = default;
  ParameterSourceTemplate& operator=(const ParameterSourceTemplate&) = delete;
  ParameterSourceTemplate& operator=(ParameterSourceTemplate&&) = delete;
  ~ParameterSourceTemplate() = default;

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
  GenerationTemplate(const GenerationTemplate&) = default;
  GenerationTemplate(GenerationTemplate&&) = default;
  GenerationTemplate& operator=(const GenerationTemplate&) = delete;
  GenerationTemplate& operator=(GenerationTemplate&&) = delete;
  ~GenerationTemplate() = default;

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
  SourceTemplate(const SourceTemplate&) = default;
  SourceTemplate(SourceTemplate&&) = default;
  SourceTemplate& operator=(const SourceTemplate&) = delete;
  SourceTemplate& operator=(SourceTemplate&&) = delete;
  ~SourceTemplate() = default;

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
  AttachToSourcesTemplate(const AttachToSourcesTemplate&) = default;
  AttachToSourcesTemplate(AttachToSourcesTemplate&&) = default;
  AttachToSourcesTemplate& operator=(const AttachToSourcesTemplate&) = delete;
  AttachToSourcesTemplate& operator=(AttachToSourcesTemplate&&) = delete;
  ~AttachToSourcesTemplate() = default;

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
  AttachToSinksTemplate(const AttachToSinksTemplate&) = default;
  AttachToSinksTemplate(AttachToSinksTemplate&&) = default;
  AttachToSinksTemplate& operator=(const AttachToSinksTemplate&) = delete;
  AttachToSinksTemplate& operator=(AttachToSinksTemplate&&) = delete;
  ~AttachToSinksTemplate() = default;

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
  AttachToPropagationsTemplate(const AttachToPropagationsTemplate&) = default;
  AttachToPropagationsTemplate(AttachToPropagationsTemplate&&) = default;
  AttachToPropagationsTemplate& operator=(const AttachToPropagationsTemplate&) =
      delete;
  AttachToPropagationsTemplate& operator=(AttachToPropagationsTemplate&&) =
      delete;
  ~AttachToPropagationsTemplate() = default;

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
  AddFeaturesToArgumentsTemplate(const AddFeaturesToArgumentsTemplate&) =
      default;
  AddFeaturesToArgumentsTemplate(AddFeaturesToArgumentsTemplate&&) = default;
  AddFeaturesToArgumentsTemplate& operator=(
      const AddFeaturesToArgumentsTemplate&) = delete;
  AddFeaturesToArgumentsTemplate& operator=(AddFeaturesToArgumentsTemplate&&) =
      delete;
  ~AddFeaturesToArgumentsTemplate() = default;

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
      std::unique_ptr<AllOfTypeConstraint> constraints,
      std::string variable,
      std::vector<SinkTemplate> sink_templates = {},
      std::vector<ParameterSourceTemplate> parameter_source_templates = {},
      std::vector<GenerationTemplate> generation_templates = {},
      std::vector<SourceTemplate> source_templates = {},
      std::vector<PropagationTemplate> propagation_templates = {},
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

  static ForAllParameters from_json(const Json::Value&, Context& context);

  /* Update a model with new sinks/generations/... when the model is/will be
   * instantiated with the method. Return true if the model was updated. */
  bool instantiate(Model& model, const Method* method) const;

 private:
  std::unique_ptr<AllOfTypeConstraint> constraints_;
  std::string variable_;
  std::vector<SinkTemplate> sink_templates_;
  std::vector<ParameterSourceTemplate> parameter_source_templates_;
  std::vector<GenerationTemplate> generation_templates_;
  std::vector<SourceTemplate> source_templates_;
  std::vector<PropagationTemplate> propagation_templates_;
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

  /* Create a model with information that is associated with a method (e.g. new
   * sinks/generations/...). */
  std::optional<Model> instantiate(Context& context, const Method* method)
      const;
  static ModelTemplate from_json(
      const Json::Value& model_generator,
      Context& context);

 private:
  Model model_;
  std::vector<ForAllParameters> for_all_parameters_;
};
} // namespace marianatrench

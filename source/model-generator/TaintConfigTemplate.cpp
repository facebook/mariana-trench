/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/model-generator/ModelTemplates.h>
#include <mariana-trench/model-generator/TaintConfigTemplate.h>

namespace marianatrench {

TaintConfigTemplate TaintConfigTemplate::from_json(const Json::Value& value, Context& context) {
  TaintConfig taint_config = TaintConfig::from_json(value, context);
  std::vector<AnnotationFeatureTemplate> annotation_features;
  if (value.isMember("via_annotation")) {
    for (const auto& feature_value : JsonValidation::null_or_array(value, "via_annotation")) {
      annotation_features.emplace_back(AnnotationFeatureTemplate::from_json(feature_value));
    }
  }
  return TaintConfigTemplate{std::move(taint_config), std::move(annotation_features)};
}

TaintConfig TaintConfigTemplate::instantiate(const Method* method, Context& context) const {
  return instantiate(method, context, TemplateVariableMapping{});
}

TaintConfig TaintConfigTemplate::instantiate(const Method* method, Context& context, const TemplateVariableMapping& parameter_positions) const {
  FeatureSet new_user_features;
  for (const auto& annotation_feature : annotation_features_) {
    const Feature* MT_NULLABLE new_feature = annotation_feature.instantiate(method, context, parameter_positions);
    if (new_feature) {
      new_user_features.add(new_feature);
    }
  }

  TaintConfig taint_config{taint_config_};
  taint_config.add_user_feature_set(new_user_features);
  return taint_config;
}

TaintConfigTemplate::TaintConfigTemplate(TaintConfig taint_config, std::vector<AnnotationFeatureTemplate> annotation_features) : taint_config_(std::move(taint_config)), annotation_features_(std::move(annotation_features)) { }

bool TaintConfigTemplate::is_concrete(const Json::Value& value) {
  return !is_template(value);
}

bool TaintConfigTemplate::is_template(const Json::Value& value) {
  return value.isMember("via_annotation");
}

} // namespace marianatrench

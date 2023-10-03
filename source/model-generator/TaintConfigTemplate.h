/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/TaintConfig.h>

#include <mariana-trench/model-generator/AnnotationFeatureTemplate.h>

namespace marianatrench {

/**
 * Template for a TaintConfig which needs to be instantiated with additional
 * context. Currently only the presence of annotation features causes a JSON
 * taint configuration to be parsed into a template rather than a regular taint
 * config.
 */
class TaintConfigTemplate final {
 public:

  static TaintConfigTemplate from_json(const Json::Value& value, Context& context);

  /** Create a concrete taint config for \p method from this template. */
  TaintConfig instantiate(const Method* method, Context& context) const;

  /**
   * Used in for_all_parameters. Creates a concrete taint config for \p method
   * from this template.
   */
  TaintConfig instantiate(const Method* method, Context& context, const class TemplateVariableMapping& parameter_positions) const;

  /**
   * true iff the taint configuration \p value has no annotation features and
   * can be read immediately as a complete taint config.
   */
  static bool is_concrete(const Json::Value& value);

  /**
   * true iff the taint configuration \p value has annotation features and must
   * be read as a taint config template for instantiation against a method.
   */
  static bool is_template(const Json::Value& value);

 private:
  TaintConfigTemplate(TaintConfig taint_config, std::vector<AnnotationFeatureTemplate> annotation_features);

  // Taint config options which do not depend on concrete method.
  TaintConfig taint_config_;

  // Annotation features to be converted to user features on instantiation.
  std::vector<AnnotationFeatureTemplate> annotation_features_;
};

} // namespace marianatrench

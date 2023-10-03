/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/model-generator/RootTemplate.h>

namespace marianatrench {

/**
 * Indicates at which location we expect the annotation from which we take the
 * value for generated user features. This allows us to use values of class,
 * method, or parameter annotations as user features.
 *
 * Corresponds to
 * https://docs.oracle.com/javase/8/docs/api/java/lang/annotation/Target.html.
 */
class AnnotationTarget {
 public:
  enum class Kind {
    /** Expected annotation is a class-level annotation. */
    Class = 0,
    /** Expected annotation is a method or return type annotation. */
    Method = 1,
    /** Expected annotation is a parameter annotation. Implies that argument()
       must be present. */
    Parameter = 2
  };

  static AnnotationTarget from_json(const Json::Value& value);

  /** Whether the expected annotation is a class, method, or parameter
   * annotation. */
  Kind kind() const;

  /**
   * Identifies the parameter where the annotation is located if kind() is
   * Kind::Parameter. Supports variable substitution for use in
   * for_all_parameters sections.
   */
  const RootTemplate& argument() const;

 private:
  AnnotationTarget(Kind kind, std::optional<RootTemplate> argument);

  Kind kind_;
  std::optional<RootTemplate> argument_;
};

/**
 * Annotation features are mapped to regular user features at model template
 * instantiation time. They consist of an annotation type to find and a root
 * port to indicate whether it is a method annotation (Root::Return) or one of
 * its parameters. The user feature content is set to the content of the
 * annotation's `value()` parameter, if present.
 */
class AnnotationFeatureTemplate {
 public:
  static AnnotationFeatureTemplate from_json(const Json::Value& value);

  /**
   * Converts the annotation feature to a concrete user feature based on
   * \p method's annotations.
   */
  const class Feature* MT_NULLABLE
  instantiate(const Method* method, class Context& context,
              const TemplateVariableMapping& parameter_positions) const;

 private:
  AnnotationFeatureTemplate(
      AnnotationTarget port, const class DexType* dex_type,
      const DexString* MT_NULLABLE tag,
      const DexString* MT_NULLABLE annotation_parameter_name);

  AnnotationTarget target_;
  const DexType* dex_type_;
  const DexString* MT_NULLABLE tag_;
  const DexString* MT_NULLABLE annotation_parameter_name_;
};

}  // namespace marianatrench

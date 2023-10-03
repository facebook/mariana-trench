/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <functional>

#include <mariana-trench/Access.h>

namespace marianatrench {

/**
 * Annotation features are mapped to regular user features at model template
 * instantiation time. They consist of an annotation type to find and a root
 * port to indicate whether it is a method annotation (Root::Return) or one of
 * its parameters. The usere label content is set to the content of the
 * annotation's `value()` parameter, if present.
 */
class AnnotationFeature {
 public:
  /** Public for testing purposes only, prod only uses from_json. */
  AnnotationFeature(Root port, const class DexType* dex_type, std::optional<std::string> label);

  /**
   * Parses the given JSON object, returning a feature created using a
   * UniquePointerFactory.
   */
  static const AnnotationFeature* from_json(const Json::Value& value, class Context& context);

  /** Used to store in a UniquePointerFactory. */
  std::size_t hash() const;
  bool operator==(const AnnotationFeature& other) const;

  /**
   * Annotation location. Root::Return for method, Root::Argument for
   * parameters.
   */
  const Root& port() const;

  /** Type of the annotation. */
  const DexType* dex_type() const;

  /** Label to use for annotation feature value. */
  const std::optional<std::string>& label() const;

  friend std::ostream& operator<<(
      std::ostream& out,
      const AnnotationFeature& annotation_feature);

 private:
  Root port_;
  const DexType* dex_type_;
  std::optional<std::string> label_;
};

}

template <>
struct std::hash<marianatrench::AnnotationFeature> {
  std::size_t operator()(const marianatrench::AnnotationFeature& annotation_feature) const {
    return annotation_feature.hash();
  }
};

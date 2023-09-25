/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sparta/AbstractDomain.h>

#include <mariana-trench/AnnotationFeature.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/PatriciaTreeSetAbstractDomain.h>

namespace marianatrench {

/**
 * Used to store annotation features in a TaintConfig. Annotation features are
 * incomplete user feature templates, and are thus not copied to a Frame.
 * Instead they are instantiated as user features during template model
 * instantiation.
 */
class AnnotationFeatureSet final : public sparta::AbstractDomain<AnnotationFeatureSet> {
 private:
  using Set = PatriciaTreeSetAbstractDomain<
      const AnnotationFeature*,
      /* bottom_is_empty */ true,
      /* with_top */ false>;

 public:
  INCLUDE_SET_MEMBER_TYPES(Set, const AnnotationFeature*)

 private:
  explicit AnnotationFeatureSet(Set set);

 public:
  /* Create the bottom (i.e, empty) feature set. */
  AnnotationFeatureSet() = default;

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(AnnotationFeatureSet)
  INCLUDE_ABSTRACT_DOMAIN_METHODS(AnnotationFeatureSet, Set, set_)
  INCLUDE_SET_METHODS(AnnotationFeatureSet, Set, set_, const AnnotationFeature*, iterator)

  static AnnotationFeatureSet from_json(const Json::Value& value, Context& context);

  friend std::ostream& operator<<(
      std::ostream& out,
      const AnnotationFeatureSet& annotation_features);

 public:
  Set set_;
};

} // namespace marianatrench

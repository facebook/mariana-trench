/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/AnnotationFeatureSet.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Show.h>

namespace marianatrench {

AnnotationFeatureSet::AnnotationFeatureSet(Set set) : set_(std::move(set)) {}

AnnotationFeatureSet AnnotationFeatureSet::from_json(const Json::Value& value, Context& context) {
  AnnotationFeatureSet features;
  for (const auto& feature_value : JsonValidation::null_or_array(value)) {
    features.add(AnnotationFeature::from_json(feature_value, context));
  }
  return features;
}

std::ostream& operator<<(std::ostream& out, const AnnotationFeatureSet& annotation_features) {
    return show_set(out, annotation_features);
}

} // namespace marianatrench

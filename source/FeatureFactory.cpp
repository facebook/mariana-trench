/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <fmt/format.h>

#include <mariana-trench/FeatureFactory.h>

namespace marianatrench {

const Feature* FeatureFactory::get(const std::string& data) const {
  return factory_.create(data);
}

const Feature* FeatureFactory::get_via_type_of_feature(
    const DexType* MT_NULLABLE type,
    const DexString* MT_NULLABLE tag) const {
  std::string tag_string = "";
  if (tag != nullptr) {
    tag_string = fmt::format("{}-", tag->c_str());
  }
  const auto& type_string = type ? type->str() : "unknown";
  return factory_.create(fmt::format("via-{}type:{}", tag_string, type_string));
}

const Feature* FeatureFactory::get_via_cast_feature(
    const DexType* MT_NULLABLE type) const {
  const auto& type_string = type ? type->str() : "unknown";
  return factory_.create("via-cast:" + type_string);
}

const Feature* FeatureFactory::get_via_value_of_feature(
    const std::optional<std::string_view>& value,
    const DexString* MT_NULLABLE tag) const {
  std::string tag_string = "";
  if (tag != nullptr) {
    tag_string = fmt::format("{}-", tag->c_str());
  }
  const auto& via_value = value.value_or("unknown");
  return factory_.create(fmt::format("via-{}value:{}", tag_string, via_value));
}

const Feature* FeatureFactory::get_via_shim_feature(
    const Method* MT_NULLABLE method) const {
  const auto& method_string = method ? method->signature() : "unknown";
  return factory_.create("via-shim:" + method_string);
}

const Feature* FeatureFactory::get_intent_routing_feature() const {
  return factory_.create("via-intent-routing");
}

const Feature* FeatureFactory::get_issue_broadening_feature() const {
  return factory_.create("via-issue-broadening");
}

const Feature* FeatureFactory::get_propagation_broadening_feature() const {
  return factory_.create("via-propagation-broadening");
}

const Feature* FeatureFactory::get_widen_broadening_feature() const {
  return factory_.create("via-widen-broadening");
}

const Feature* FeatureFactory::get_invalid_path_broadening() const {
  return factory_.create("via-invalid-path-broadening");
}

const AnnotationFeature* FeatureFactory::get_unique_annotation_feature(const AnnotationFeature& annotation_feature) const {
  return annotation_factory_.create(annotation_feature);
}

const FeatureFactory& FeatureFactory::singleton() {
  // Thread-safe global variable, initialized on first call.
  static FeatureFactory instance;
  return instance;
}

} // namespace marianatrench

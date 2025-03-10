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

/**
 * Helper to format a feature with optional values and labels. Used for
 * via-value with labels and annotation features.
 */
static const Feature* get_labelled_feature(
    const UniquePointerFactory<std::string, Feature>& factory,
    const char* const format_str,
    const std::optional<std::string_view>& value,
    const DexString* const MT_NULLABLE tag) {
  std::string tag_string = "";
  if (tag != nullptr) {
    tag_string = fmt::format("{}-", tag->c_str());
  }
  const auto& via_value = value.value_or("unknown");
  return factory.create(fmt::format(format_str, tag_string, via_value));
}

const Feature* FeatureFactory::get_via_type_of_feature(
    const DexType* MT_NULLABLE type,
    const DexString* MT_NULLABLE tag) const {
  std::optional<std::string_view> type_string;
  if (type) {
    type_string = type->str();
  }
  return get_labelled_feature(factory_, "via-{}type:{}", type_string, tag);
}

const Feature* FeatureFactory::get_via_cast_feature(
    const DexType* MT_NULLABLE type) const {
  const auto& type_string = type ? type->str() : "unknown";
  return factory_.create("via-cast:" + type_string);
}

const Feature* FeatureFactory::get_via_value_of_feature(
    const std::optional<std::string_view>& value,
    const DexString* MT_NULLABLE tag) const {
  return get_labelled_feature(factory_, "via-{}value:{}", value, tag);
}

const Feature* FeatureFactory::get_via_annotation_feature(
    const std::string_view value,
    const DexString* const MT_NULLABLE tag) const {
  return get_labelled_feature(factory_, "via-{}annotation:{}", value, tag);
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

const Feature* FeatureFactory::get_alias_broadening_feature() const {
  return factory_.create("via-alias-widen-broadening");
}

const Feature* FeatureFactory::get_invalid_path_broadening() const {
  return factory_.create("via-invalid-path-broadening");
}

const Feature* FeatureFactory::get_missing_method() const {
  return factory_.create("via-missing-method");
}

const Feature* FeatureFactory::get_exploitability_root() const {
  return factory_.create("exploitability-root-callable");
}

const Feature* FeatureFactory::get_exploitability_origin_feature(
    const ExploitabilityOrigin* exploitability_origin) const {
  mt_assert(exploitability_origin != nullptr);
  auto callee = exploitability_origin->callee()->str_copy();

  // This feature is used for filtering in the SAPP UI which only includes the
  // callee name but not its arguments.
  return factory_.create(callee.substr(0, callee.find(':')));
}

const FeatureFactory& FeatureFactory::singleton() {
  // Thread-safe global variable, initialized on first call.
  static FeatureFactory instance;
  return instance;
}

} // namespace marianatrench

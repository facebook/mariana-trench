/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <DexStore.h>
#include <RedexResources.h>

#include <mariana-trench/Dependencies.h>
#include <mariana-trench/Feature.h>
#include <mariana-trench/FeatureMayAlwaysSet.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Options.h>

namespace marianatrench {

class ClassProperties final {
 public:
  explicit ClassProperties(
      const Options& options,
      const DexStoresVector& stores,
      const Features& features,
      const Dependencies& dependencies,
      std::unique_ptr<AndroidResources> android_resources = nullptr);
  ClassProperties(const ClassProperties&) = delete;
  ClassProperties(ClassProperties&&) = delete;
  ClassProperties& operator=(const ClassProperties&) = delete;
  ClassProperties& operator=(ClassProperties&&) = delete;
  ~ClassProperties() = default;

 private:
  bool is_class_unexported(std::string_view class_name) const;
  bool is_class_exported(std::string_view class_name) const;
  bool is_child_exposed(std::string_view class_name) const;
  bool is_dfa_public(std::string_view class_name) const;
  bool has_protection_level(std::string_view class_name) const;
  bool has_permission(std::string_view class_name) const;
  std::optional<std::string> get_privacy_decision_number_from_class_name(
      std::string_view class_name) const;
  std::optional<std::string> get_privacy_decision_number_from_method(
      const Method* method) const;

  bool has_user_exposed_properties(std::string_view class_name) const;
  bool has_user_unexposed_properties(std::string_view class_name) const;
  FeatureSet get_class_features(std::string_view clazz, bool via_dependency)
      const;
  FeatureSet compute_transitive_class_features(const Method* method) const;

 public:
  /* A set of features to add to sources propagated from callee to caller. */
  FeatureMayAlwaysSet propagate_features(
      const Method* caller,
      const Method* callee,
      const Features& features) const;

  /* A set of features to add on issues found in the given method. */
  FeatureMayAlwaysSet issue_features(const Method* method) const;

 private:
  std::unordered_set<std::string_view> exported_classes_;
  std::unordered_set<std::string_view> unexported_classes_;
  std::unordered_set<std::string_view> parent_exposed_classes_;
  std::unordered_set<std::string_view> dfa_public_scheme_classes_;
  std::unordered_set<std::string_view> permission_classes_;
  std::unordered_set<std::string_view> protection_level_classes_;
  std::unordered_map<std::string_view, std::string> privacy_decision_classes_;

  // Note: This is not thread-safe.
  StringStorage strings_;

  const Features& features_;
  const Dependencies& dependencies_;
  // This is a cache and updating it does not change the internal state of the
  // object and hence is safe to mark as mutable.
  mutable ConcurrentMap<const Method*, const Method*> via_dependencies_;
};

} // namespace marianatrench

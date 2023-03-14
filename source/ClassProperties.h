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
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Options.h>

namespace marianatrench {

enum class ExportedKind {
  ExportedWithPermission,
  Exported,
  Unexported,
};

class ClassProperties final {
 public:
  explicit ClassProperties(
      const Options& options,
      const DexStoresVector& stores,
      const Features& features,
      const Dependencies& dependencies,
      std::unique_ptr<AndroidResources> android_resources = nullptr);

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ClassProperties)

 private:
  void emplace_classes(
      std::unordered_map<std::string_view, ExportedKind>& map,
      const ComponentTagInfo& tag_info);
  FeatureSet get_manifest_features(
      std::string_view class_name,
      const std::unordered_map<std::string_view, ExportedKind>& component_set)
      const;
  bool is_dfa_public(std::string_view class_name) const;
  bool has_inline_permissions(std::string_view class_name) const;
  bool has_privacy_decision(const Method* method) const;

  FeatureSet get_class_features(
      std::string_view clazz,
      const NamedKind* kind,
      bool via_dependency,
      size_t dependency_depth = 0) const;
  FeatureSet compute_transitive_class_features(
      const Method* method,
      const NamedKind* kind) const;

 public:
  /* A set of features to add to sources propagated from callee to caller. */
  FeatureMayAlwaysSet propagate_features(
      const Method* caller,
      const Method* callee,
      const Features& features) const;

  /* A set of features to add on issues found in the given method. */
  FeatureMayAlwaysSet issue_features(
      const Method* method,
      std::unordered_set<const Kind*> kinds) const;

  static DexClass* MT_NULLABLE get_service_from_stub(const DexClass* clazz);

 private:
  std::unordered_map<std::string_view, ExportedKind> activities_;
  std::unordered_map<std::string_view, ExportedKind> services_;
  std::unordered_map<std::string_view, ExportedKind> receivers_;
  std::unordered_map<std::string_view, ExportedKind> providers_;

  std::unordered_set<std::string_view> dfa_public_scheme_classes_;
  std::unordered_set<std::string_view> inline_permission_classes_;
  std::unordered_set<std::string_view> privacy_decision_classes_;

  // Note: This is not thread-safe.
  StringStorage strings_;

  const Features& features_;
  const Dependencies& dependencies_;
  // This is a cache and updating it does not change the internal state of the
  // object and hence is safe to mark as mutable.
  mutable ConcurrentMap<const Method*, const Method*> via_dependencies_;
};

} // namespace marianatrench

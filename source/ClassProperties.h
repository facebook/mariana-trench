// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <DexStore.h>

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
      const Features& features);
  ClassProperties(const ClassProperties&) = delete;
  ClassProperties(ClassProperties&&) = delete;
  ClassProperties& operator=(const ClassProperties&) = delete;
  ClassProperties& operator=(ClassProperties&&) = delete;
  ~ClassProperties() = default;

  bool is_class_unexported(const std::string& class_name) const;
  bool is_class_exported(const std::string& class_name) const;
  bool is_child_exported(const std::string& class_name) const;
  bool is_dfa_public(const std::string& class_name) const;
  bool has_protection_level(const std::string& class_name) const;
  bool has_child_protection_level(const std::string& class_name) const;
  bool has_permission(const std::string& class_name) const;
  bool has_child_permission(const std::string& class_name) const;
  std::optional<std::string> get_privacy_decision_number_from_class_name(
      const std::string& class_name) const;
  std::optional<std::string> get_privacy_decision_number_from_method(
      const Method* method) const;

  /* A set of features to add to sources propagated from callee to caller. */
  FeatureMayAlwaysSet propagate_features(
      const Method* caller,
      const Method* callee,
      const Features& features) const;

  /* A set of features to add to parameter sources. */
  FeatureMayAlwaysSet parameter_source_features(const Method* method) const;

 private:
  std::unordered_set<std::string> exported_classes_;
  std::unordered_set<std::string> unexported_classes_;
  std::unordered_set<std::string> parent_exported_classes_;
  std::unordered_set<std::string> dfa_public_scheme_classes_;
  std::unordered_set<std::string> permission_classes_;
  std::unordered_set<std::string> parent_permission_classes_;
  std::unordered_set<std::string> protection_level_classes_;
  std::unordered_set<std::string> parent_protection_level_classes_;
  std::unordered_map<std::string, std::string> privacy_decision_classes_;

  const Feature* via_caller_exported_;
  const Feature* via_child_exported_;
  const Feature* via_caller_unexported_;
  const Feature* via_public_dfa_scheme_;
  const Feature* via_caller_permission_;
  const Feature* via_child_permission_;
  const Feature* via_caller_protection_level_;
  const Feature* via_child_protection_level_;
};

} // namespace marianatrench

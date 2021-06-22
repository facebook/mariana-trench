/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <exception>
#include <mutex>

#include <InstructionAnalyzer.h>
#include <RedexResources.h>
#include <Walkers.h>
#include <re2/re2.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/ClassProperties.h>
#include <mariana-trench/Constants.h>
#include <mariana-trench/Features.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/ModelGenerator.h>

namespace {

// Various component sources are not matching the class names in the
// manifest leading to features (like exported) not being added.
std::string strip_inner_class(std::string class_name) {
  auto position = class_name.find_first_of("$");
  if (position != std::string::npos) {
    return class_name.substr(0, position) + ";";
  }
  return class_name;
}

bool is_class_exported_via_uri(const DexClass* clazz) {
  if (!clazz->get_anno_set()) {
    return false;
  }

  auto dfa_annotation = marianatrench::constants::get_dfa_annotation();
  auto private_schemes = marianatrench::constants::get_private_uri_schemes();

  for (const DexAnnotation* annotation :
       clazz->get_anno_set()->get_annotations()) {
    if (!annotation->type() ||
        annotation->type()->str() != dfa_annotation.type) {
      continue;
    }

    for (const DexAnnotationElement& element : annotation->anno_elems()) {
      if (element.string->str() != "value") {
        continue;
      }

      auto* patterns =
          dynamic_cast<DexEncodedValueArray*>(element.encoded_value);
      mt_assert(patterns != nullptr);

      for (DexEncodedValue* encoded_pattern : *patterns->evalues()) {
        auto* pattern =
            dynamic_cast<DexEncodedValueAnnotation*>(encoded_pattern);
        mt_assert(pattern != nullptr);

        if (!pattern->type() ||
            pattern->type()->str() != dfa_annotation.pattern_type) {
          continue;
        }

        std::string pattern_value = pattern->show();

        // We only care about patterns that specify a scheme or a pattern
        if (pattern_value.find("scheme") == std::string::npos &&
            pattern_value.find("pattern") == std::string::npos) {
          continue;
        }

        if (!std::any_of(
                private_schemes.begin(),
                private_schemes.end(),
                [&](std::string scheme) {
                  return pattern_value.find(scheme) != std::string::npos;
                })) {
          LOG(2,
              "Class {} has DFA annotations with a public URI scheme.",
              clazz->get_name()->str());
          return true;
        }
      }
    }
  }

  return false;
}

std::optional<std::string> get_privacy_decision_number_from_annotations(
    const std::vector<DexAnnotation*>& annotations) {
  auto privacy_decision_type =
      marianatrench::constants::get_privacy_decision_type();
  for (const DexAnnotation* annotation : annotations) {
    if (!annotation->type() ||
        annotation->type()->str() != privacy_decision_type) {
      continue;
    }

    for (const DexAnnotationElement& element : annotation->anno_elems()) {
      if (element.string->str() != "value") {
        continue;
      } else {
        return element.encoded_value->show();
      }
    }
  }
  return std::nullopt;
}

std::optional<std::string> get_privacy_decision_number_from_class(
    const DexClass* clazz) {
  if (!clazz->get_anno_set()) {
    return std::nullopt;
  }

  return get_privacy_decision_number_from_annotations(
      clazz->get_anno_set()->get_annotations());
}

std::unordered_set<std::string> get_class_fragments(const DexClass* clazz) {
  std::unordered_set<std::string> exported_fragments;
  const static re2::RE2 fragment_regex = re2::RE2("(L[^a][^; ]*Fragment;)");
  auto methods = clazz->get_all_methods();
  for (DexMethod* method : methods) {
    const auto& code = method->get_code();
    if (!code) {
      continue;
    }
    const cfg::ControlFlowGraph& cfg = code->cfg();
    for (const auto* block : cfg.blocks()) {
      std::string match;
      // [0x7f5380273670] OPCODE: INVOKE_STATIC
      // Lcom/example/myapplication/ui/main/MainFragment;.newIn[...]
      // Here we match the class of any Fragment
      if (re2::RE2::PartialMatch(show(block), fragment_regex, &match)) {
        exported_fragments.emplace(match);
      }
    }
  }
  return exported_fragments;
}

} // namespace

namespace marianatrench {

ClassProperties::ClassProperties(
    const Options& options,
    const DexStoresVector& stores,
    const Features& features)
    : features_(features) {
  try {
    auto android_resources = create_resource_reader(options.apk_directory());
    const auto manifest_class_info =
        android_resources->get_manifest_class_info();

    for (const auto& tag_info : manifest_class_info.component_tags) {
      std::unordered_set<std::string> parent_classes;
      auto dex_class = redex::get_class(tag_info.classname);
      bool protection_level = false;
      bool permission = false;

      if (!tag_info.protection_level.empty() &&
          tag_info.protection_level != "normal") {
        protection_level_classes_.emplace(tag_info.classname);
        protection_level = true;
      }
      if (!tag_info.permission.empty()) {
        permission_classes_.emplace(tag_info.classname);
        permission = true;
      }
      if (tag_info.is_exported == BooleanXMLAttribute::True ||
          (tag_info.is_exported == BooleanXMLAttribute::Undefined &&
           tag_info.has_intent_filters)) {
        exported_classes_.emplace(tag_info.classname);
        if (!protection_level && !permission && dex_class) {
          if (tag_info.tag == ComponentTag::Activity) {
            const auto& exported_fragments = get_class_fragments(dex_class);
            exported_classes_.insert(
                exported_fragments.begin(), exported_fragments.end());
          }
          parent_classes = generator::get_custom_parents_from_class(dex_class);
          parent_exposed_classes_.insert(
              parent_classes.begin(), parent_classes.end());
        }
      } else {
        unexported_classes_.emplace(tag_info.classname);
      }
    }
  } catch (const std::exception& e) {
    // Redex may assert, or throw `std::runtime_error` if the file is missing.
    ERROR(2, "Manifest could not be parsed: {}", e.what());
  }

  std::mutex mutex;
  for (auto& scope : DexStoreClassesIterator(stores)) {
    walk::parallel::classes(scope, [&](DexClass* clazz) {
      if (is_class_exported_via_uri(clazz)) {
        std::lock_guard<std::mutex> lock(mutex);
        dfa_public_scheme_classes_.emplace(clazz->str());
      }

      std::optional<std::string> privacy_decision_number =
          get_privacy_decision_number_from_class(clazz);
      if (privacy_decision_number) {
        std::lock_guard<std::mutex> lock(mutex);
        privacy_decision_classes_.emplace(
            clazz->str(), *privacy_decision_number);
      }
    });
  }
}

bool ClassProperties::is_class_exported(const std::string& class_name) const {
  auto outer_class = strip_inner_class(class_name);
  return exported_classes_.count(class_name) > 0 ||
      exported_classes_.count(outer_class) > 0;
}

bool ClassProperties::is_child_exposed(const std::string& class_name) const {
  auto outer_class = strip_inner_class(class_name);
  return parent_exposed_classes_.count(class_name) > 0 ||
      parent_exposed_classes_.count(outer_class) > 0;
}

bool ClassProperties::is_class_unexported(const std::string& class_name) const {
  auto outer_class = strip_inner_class(class_name);
  return unexported_classes_.count(class_name) > 0 ||
      unexported_classes_.count(outer_class) > 0;
}

bool ClassProperties::is_dfa_public(const std::string& class_name) const {
  auto outer_class = strip_inner_class(class_name);
  return dfa_public_scheme_classes_.count(class_name) > 0 ||
      dfa_public_scheme_classes_.count(outer_class) > 0;
}

bool ClassProperties::has_protection_level(
    const std::string& class_name) const {
  auto outer_class = strip_inner_class(class_name);
  return protection_level_classes_.count(class_name) > 0 ||
      protection_level_classes_.count(outer_class) > 0;
}

bool ClassProperties::has_permission(const std::string& class_name) const {
  auto outer_class = strip_inner_class(class_name);
  return permission_classes_.count(class_name) > 0 ||
      permission_classes_.count(outer_class) > 0;
  ;
}

std::optional<std::string>
ClassProperties::get_privacy_decision_number_from_class_name(
    const std::string& class_name) const {
  auto it = privacy_decision_classes_.find(class_name);
  if (it != privacy_decision_classes_.end()) {
    return it->second;
  } else {
    return std::nullopt;
  }
}

std::optional<std::string>
ClassProperties::get_privacy_decision_number_from_method(
    const Method* method) const {
  std::optional<std::string> privacy_decision_number = std::nullopt;
  // add the annotation from the enclosing method by default
  if (method->dex_method()->get_anno_set()) {
    privacy_decision_number = get_privacy_decision_number_from_annotations(
        method->dex_method()->get_anno_set()->get_annotations());
  }

  if (!privacy_decision_number) {
    privacy_decision_number =
        get_privacy_decision_number_from_class_name(method->get_class()->str());
  }

  return privacy_decision_number;
}

FeatureMayAlwaysSet ClassProperties::propagate_features(
    const Method* caller,
    const Method* /*callee*/,
    const Features& feature_factory) const {
  FeatureSet features;

  std::optional<std::string> privacy_decision_number =
      get_privacy_decision_number_from_method(caller);

  if (privacy_decision_number) {
    features.add(feature_factory.get("pd-" + *privacy_decision_number));
  }

  return FeatureMayAlwaysSet::make_always(features);
}

FeatureMayAlwaysSet ClassProperties::issue_features(
    const Method* method) const {
  FeatureSet features;
  auto clazz = method->get_class()->str();

  if (is_class_exported(clazz)) {
    features.add(features_.get("via-caller-exported"));
  }
  if (is_child_exposed(clazz)) {
    features.add(features_.get("via-child-exposed"));
  }
  if (is_class_unexported(clazz)) {
    features.add(features_.get("via-caller-unexported"));
  }
  if (is_dfa_public(clazz)) {
    features.add(features_.get("via-public-dfa-scheme"));
  }
  if (has_permission(clazz)) {
    features.add(features_.get("via-caller-permission"));
  }
  if (has_protection_level(clazz)) {
    features.add(features_.get("via-caller-protection-level"));
  }

  return FeatureMayAlwaysSet::make_always(features);
}

} // namespace marianatrench

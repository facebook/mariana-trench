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
#include <mariana-trench/Features.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/ModelGenerator.h>

namespace {

// Various component sources are not matching the class names in the
// manifest leading to features (like exported) not being added.
std::string strip_subclass(std::string class_name) {
  const static re2::RE2 anonymous_class_regex = re2::RE2("(\\$[0-9]+)+;");
  const static re2::RE2 service_regex =
      re2::RE2("Service[^;]*\\$[^;]*(Handler|Stub)");

  re2::RE2::Replace(&class_name, anonymous_class_regex, ";");
  if (boost::contains(class_name, "Provider$Impl")) {
    auto position = class_name.find_first_of("$");
    if (position != std::string::npos) {
      return class_name.substr(0, position) + ";";
    }
  }
  if (re2::RE2::PartialMatch(class_name, service_regex)) {
    auto position = class_name.find_first_of("$");
    if (position != std::string::npos) {
      return class_name.substr(0, position) + ";";
    }
  }
  return class_name;
}

bool is_class_exported_via_uri(const DexClass* clazz) {
  if (!clazz->get_anno_set()) {
    return false;
  }

  const std::string dfa_annotation_type =
      "Lcom/facebook/crudolib/urimap/UriMatchPatterns;";
  const std::string dfa_annotation_pattern_type =
      "Lcom/facebook/crudolib/urimap/UriPattern;";

  // TODO (T57415229): keep this list in sync with
  // https://our.internmc.facebook.com/intern/diffusion/FBS/browse/master/fbandroid/java/com/facebook/crudolib/urimap/UriScheme.java
  const std::vector<std::string> private_schemes = {
      "scheme:Lcom/facebook/crudolib/urimap/UriScheme;.FBINTERNAL:Lcom/facebook/crudolib/urimap/UriScheme;",
      "scheme:Lcom/facebook/crudolib/urimap/UriScheme;.MESSENGER_SECURE:Lcom/facebook/crudolib/urimap/UriScheme;",
      "scheme:Lcom/facebook/crudolib/urimap/UriScheme;.MLITE_SECURE:Lcom/facebook/crudolib/urimap/UriScheme;",
      "scheme:Lcom/facebook/crudolib/urimap/UriScheme;.BIZAPP_INTERNAL:Lcom/facebook/crudolib/urimap/UriScheme;",
      "scheme:Lcom/facebook/crudolib/urimap/UriScheme;.MESSENGER_SAMETASK:Lcom/facebook/crudolib/urimap/UriScheme;",
      "pattern:fbinternal://",
  };

  for (const DexAnnotation* annotation :
       clazz->get_anno_set()->get_annotations()) {
    if (!annotation->type() ||
        annotation->type()->str() != dfa_annotation_type) {
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
            pattern->type()->str() != dfa_annotation_pattern_type) {
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
  const std::string privacy_decision_type =
      "Lcom/facebook/privacy/datacollection/PrivacyDecision;";
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
    const cfg::ControlFlowGraph& cfg = method->get_code()->cfg();
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
    : via_caller_exported_(features.get("via-caller-exported")),
      via_child_exposed_(features.get("via-child-exposed")),
      via_caller_unexported_(features.get("via-caller-unexported")),
      via_public_dfa_scheme_(features.get("via-public-dfa-scheme")),
      via_caller_permission_(features.get("via-caller-permission")),
      via_caller_protection_level_(
          features.get("via-caller-protection-level")) {
  try {
    const auto manifest_class_info = get_manifest_class_info(
        options.apk_directory() + "/AndroidManifest.xml");

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
  return exported_classes_.count(class_name) > 0;
}

bool ClassProperties::is_child_exposed(const std::string& class_name) const {
  return parent_exposed_classes_.count(class_name) > 0;
}

bool ClassProperties::is_class_unexported(const std::string& class_name) const {
  return unexported_classes_.count(class_name) > 0;
}

bool ClassProperties::is_dfa_public(const std::string& class_name) const {
  return dfa_public_scheme_classes_.count(class_name) > 0;
}

bool ClassProperties::has_protection_level(
    const std::string& class_name) const {
  return protection_level_classes_.count(class_name) > 0;
}

bool ClassProperties::has_permission(const std::string& class_name) const {
  return permission_classes_.count(class_name) > 0;
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
  auto base_class = strip_subclass(method->get_class()->str());

  if (is_class_exported(base_class)) {
    features.add(via_caller_exported_);
  }
  if (is_child_exposed(base_class)) {
    features.add(via_child_exposed_);
  }
  if (is_class_unexported(base_class)) {
    features.add(via_caller_unexported_);
  }
  if (is_dfa_public(base_class)) {
    features.add(via_public_dfa_scheme_);
  }
  if (has_permission(base_class)) {
    features.add(via_caller_permission_);
  }
  if (has_protection_level(base_class)) {
    features.add(via_caller_protection_level_);
  }

  return FeatureMayAlwaysSet::make_always(features);
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
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
#include <mariana-trench/EventLogger.h>
#include <mariana-trench/Features.h>
#include <mariana-trench/Heuristics.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/ModelGenerator.h>

namespace {

// Various component sources are not matching the class names in the
// manifest leading to features (like exported) not being added.
std::string_view strip_inner_class(std::string_view class_name) {
  auto position = class_name.find_first_of("$");
  if (position != std::string::npos) {
    return DexString::make_string(
               fmt::format("{};", class_name.substr(0, position)))
        ->str();
  }
  return class_name;
}

bool is_class_exported_via_uri(const DexClass* clazz) {
  if (!clazz->get_anno_set()) {
    return false;
  }

  auto dfa_annotation = marianatrench::constants::get_dfa_annotation();
  auto private_schemes = marianatrench::constants::get_private_uri_schemes();

  for (const auto& annotation : clazz->get_anno_set()->get_annotations()) {
    if (!annotation->type() ||
        annotation->type()->str() != dfa_annotation.type) {
      continue;
    }

    for (const DexAnnotationElement& element : annotation->anno_elems()) {
      if (element.string->str() != "value") {
        continue;
      }

      auto* patterns =
          dynamic_cast<DexEncodedValueArray*>(element.encoded_value.get());
      mt_assert(patterns != nullptr);

      for (auto& encoded_pattern : *patterns->evalues()) {
        auto* pattern =
            dynamic_cast<DexEncodedValueAnnotation*>(encoded_pattern.get());
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
    const std::vector<std::unique_ptr<DexAnnotation>>& annotations) {
  auto privacy_decision_type =
      marianatrench::constants::get_privacy_decision_type();
  for (const auto& annotation : annotations) {
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

bool has_permission_check(const DexClass* clazz) {
  auto methods = clazz->get_all_methods();
  for (DexMethod* method : methods) {
    const auto& code = method->get_code();
    if (!code) {
      continue;
    }
    const cfg::ControlFlowGraph& cfg = code->cfg();
    for (const auto* block : cfg.blocks()) {
      auto show_block = show(block);
      if (boost::contains(show_block, "TrustedCaller") ||
          boost::contains(show_block, "AbilityIPCPermissionManager") ||
          boost::contains(show_block, "CallerInfoHelper") ||
          boost::contains(show_block, "AppUpdateRequestIntentVerifier") ||
          boost::contains(show_block, "TrustManager") ||
          boost::contains(show_block, "CallingIpcPermissionManager")) {
        return true;
      }
    }
  }
  return false;
}

} // namespace

namespace marianatrench {

ClassProperties::ClassProperties(
    const Options& options,
    const DexStoresVector& stores,
    const Features& features,
    const Dependencies& dependencies,
    std::unique_ptr<AndroidResources> android_resources)
    : features_(features), dependencies_(dependencies) {
  try {
    if (android_resources == nullptr) {
      android_resources = create_resource_reader(options.apk_directory());
    }
    const auto manifest_class_info =
        android_resources->get_manifest_class_info();

    for (const auto& tag_info : manifest_class_info.component_tags) {
      std::unordered_set<std::string_view> parent_classes;
      auto dex_class = redex::get_class(tag_info.classname);
      bool permission = false;

      if (!tag_info.permission.empty()) {
        permission_classes_.emplace(strings_[tag_info.classname]);
        permission = true;
      }
      if (tag_info.is_exported == BooleanXMLAttribute::True ||
          (tag_info.is_exported == BooleanXMLAttribute::Undefined &&
           tag_info.has_intent_filters)) {
        exported_classes_.emplace(strings_[tag_info.classname]);
        if (!permission && dex_class) {
          if (tag_info.tag == ComponentTag::Activity) {
            const auto& exported_fragments = get_class_fragments(dex_class);
            for (const auto& klass : exported_fragments) {
              exposed_fragments_.emplace(
                  strings_[klass], strings_[tag_info.classname]);
            }
          }
          parent_classes = generator::get_custom_parents_from_class(dex_class);
          for (const auto& klass : parent_classes) {
            parent_exposed_classes_.emplace(
                strings_[klass], strings_[tag_info.classname]);
          }
        }
      } else {
        unexported_classes_.emplace(strings_[tag_info.classname]);
      }
    }
  } catch (const std::exception& e) {
    // Redex may assert, or throw `std::runtime_error` if the file is missing.
    auto error = fmt::format("Manifest could not be parsed: {}", e.what());
    ERROR(2, error);
    EventLogger::log_event("manifest_error", error, 1);
  }

  std::mutex mutex;
  for (auto& scope : DexStoreClassesIterator(stores)) {
    walk::parallel::classes(scope, [&](DexClass* clazz) {
      if (is_class_exported_via_uri(clazz)) {
        std::lock_guard<std::mutex> lock(mutex);
        dfa_public_scheme_classes_.emplace(clazz->str());
      }

      if (has_permission_check(clazz)) {
        std::lock_guard<std::mutex> lock(mutex);
        inline_permission_classes_.emplace(clazz->str());
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

bool ClassProperties::is_class_exported(std::string_view class_name) const {
  auto outer_class = strip_inner_class(class_name);
  return exported_classes_.count(class_name) > 0 ||
      exported_classes_.count(outer_class) > 0;
}

std::optional<std::string_view> ClassProperties::get_exposed_child(
    std::string_view parent_activity) const {
  auto lookup = parent_exposed_classes_.find(parent_activity);
  if (lookup != parent_exposed_classes_.end()) {
    return lookup->second;
  }
  auto outer_class_lookup =
      parent_exposed_classes_.find(strip_inner_class(parent_activity));
  if (outer_class_lookup != parent_exposed_classes_.end()) {
    return outer_class_lookup->second;
  }
  return std::nullopt;
}

std::optional<std::string_view> ClassProperties::get_exposed_host_activity(
    std::string_view fragment_name) const {
  auto lookup = exposed_fragments_.find(fragment_name);
  if (lookup != exposed_fragments_.end()) {
    return lookup->second;
  }
  auto outer_class_lookup =
      exposed_fragments_.find(strip_inner_class(fragment_name));
  if (outer_class_lookup != exposed_fragments_.end()) {
    return outer_class_lookup->second;
  }
  return std::nullopt;
}

bool ClassProperties::is_class_unexported(std::string_view class_name) const {
  auto outer_class = strip_inner_class(class_name);
  return unexported_classes_.count(class_name) > 0 ||
      unexported_classes_.count(outer_class) > 0;
}

bool ClassProperties::has_inline_permissions(
    std::string_view class_name) const {
  auto outer_class = strip_inner_class(class_name);
  return inline_permission_classes_.count(class_name) > 0 ||
      inline_permission_classes_.count(outer_class) > 0;
}

bool ClassProperties::is_dfa_public(std::string_view class_name) const {
  auto outer_class = strip_inner_class(class_name);
  return dfa_public_scheme_classes_.count(class_name) > 0 ||
      dfa_public_scheme_classes_.count(outer_class) > 0;
}

bool ClassProperties::has_permission(std::string_view class_name) const {
  auto outer_class = strip_inner_class(class_name);
  return permission_classes_.count(class_name) > 0 ||
      permission_classes_.count(outer_class) > 0;
  ;
}

std::optional<std::string>
ClassProperties::get_privacy_decision_number_from_class_name(
    std::string_view class_name) const {
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
  auto clazz = method->get_class()->str();
  auto features = get_class_features(clazz, /* via_dependency */ false);
  if (!has_user_exposed_properties(clazz) &&
      !has_user_unexposed_properties(clazz)) {
    features.join_with(compute_transitive_class_features(method));
  }

  return FeatureMayAlwaysSet::make_always(features);
}

FeatureSet ClassProperties::get_class_features(
    std::string_view clazz,
    bool via_dependency,
    size_t dependency_depth) const {
  FeatureSet features;

  if (is_class_exported(clazz)) {
    features.add(features_.get("via-caller-exported"));
  }
  auto exposed_child = get_exposed_child(clazz);
  if (exposed_child) {
    features.add(features_.get("via-child-exposed"));
    features.add(features_.get(fmt::format("via-class:{}", *exposed_child)));
  }
  auto exposed_host = get_exposed_host_activity(clazz);
  if (exposed_host) {
    features.add(features_.get("via-caller-exported"));
    features.add(features_.get(fmt::format("via-class:{}", *exposed_host)));
  }
  if (is_class_unexported(clazz)) {
    features.add(features_.get("via-caller-unexported"));
  }
  if (has_permission(clazz)) {
    features.add(features_.get("via-caller-permission"));
  }
  if (has_inline_permissions(clazz)) {
    features.add(features_.get("via-permission-check-in-class"));
  }

  // `via-public-dfa-scheme` feature only applies within the same class.
  if (!via_dependency && is_dfa_public(clazz)) {
    features.add(features_.get("via-public-dfa-scheme"));
  }
  if (via_dependency) {
    features.add(features_.get("via-dependency-graph"));
    features.add(features_.get(fmt::format("via-class:{}", clazz)));
    if (dependency_depth > 5) {
      features.add(features_.get("via-dependency-graph-depth-above-5"));
    }
  }

  return features;
}

bool ClassProperties::has_user_exposed_properties(
    std::string_view class_name) const {
  return is_class_exported(class_name) || get_exposed_child(class_name) ||
      get_exposed_host_activity(class_name);
};

bool ClassProperties::has_user_unexposed_properties(
    std::string_view class_name) const {
  return is_class_unexported(class_name) || has_permission(class_name);
};

namespace {

struct QueueItem {
  const Method* method;
  size_t depth;
};

} // namespace

FeatureSet ClassProperties::compute_transitive_class_features(
    const Method* callee) const {
  // Check cache
  if (const auto* target_method = via_dependencies_.get(callee, nullptr)) {
    return get_class_features(
        target_method->get_class()->str(), /* via_dependency */ true);
  }

  size_t depth = 0;
  std::queue<QueueItem> queue;
  queue.push(QueueItem{.method = callee, .depth = depth});
  std::unordered_set<const Method*> processed;
  QueueItem target{.method = nullptr, .depth = 0};

  // Traverse the dependency graph till we find closest "exported" class.
  // If no "exported" class is reachable, find the closest "unexported" class.
  // "exported" class that is only reachable via an "unexported" class is
  // ignored.
  while (!queue.empty()) {
    auto item = queue.front();
    queue.pop();
    processed.emplace(item.method);
    depth = item.depth;
    const auto class_name = item.method->get_class()->str();

    if (has_user_exposed_properties(class_name)) {
      target = item;
      break;
    }

    if (target.method == nullptr && has_user_unexposed_properties(class_name)) {
      // Continue search for user exposed properties along other paths.
      target = item;
      continue;
    }

    if (depth == Heuristics::kMaxDepthClassProperties) {
      continue;
    }

    for (const auto* dependency : dependencies_.dependencies(item.method)) {
      if (processed.count(dependency) == 0) {
        queue.push(QueueItem{.method = dependency, .depth = depth + 1});
      }
    }
  }

  if (target.method != nullptr) {
    LOG(4,
        "Class properties found for: `{}` via-dependency with `{}` at depth {}",
        show(callee),
        show(target.method),
        target.depth);
    via_dependencies_.insert({callee, target.method});
    return get_class_features(
        target.method->get_class()->str(),
        /* via_dependency */ true,
        /* depedency_length */ depth);
  }

  return FeatureSet();
}

} // namespace marianatrench

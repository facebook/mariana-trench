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
#include <mariana-trench/FeatureFactory.h>
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

bool is_manifest_relevant_kind(const std::string_view kind) {
  return (
      kind == "ActivityUserInput" || kind == "ActivityLifecycle" ||
      kind == "ReceiverUserInput" || kind == "ServiceUserInput" ||
      kind == "ServiceAIDLUserInput" || kind == "ProviderUserInput");
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

bool has_privacy_decision_annotation(
    const std::vector<std::unique_ptr<DexAnnotation>>& annotations) {
  auto privacy_decision_type =
      marianatrench::constants::get_privacy_decision_type();
  for (const auto& annotation : annotations) {
    if (annotation->type() &&
        annotation->type()->str() == privacy_decision_type) {
      return true;
    }
  }
  return false;
}

bool has_privacy_decision_in_class(const DexClass* clazz) {
  if (!clazz->get_anno_set()) {
    return false;
  }

  return has_privacy_decision_annotation(
      clazz->get_anno_set()->get_annotations());
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
    const FeatureFactory& feature_factory,
    const Dependencies& dependencies,
    std::unique_ptr<AndroidResources> android_resources)
    : feature_factory_(feature_factory), dependencies_(dependencies) {
  try {
    if (android_resources == nullptr) {
      android_resources = create_resource_reader(options.apk_directory());
    }
    const auto manifest_class_info =
        android_resources->get_manifest_class_info();

    for (const auto& tag_info : manifest_class_info.component_tags) {
      switch (tag_info.tag) {
        case ComponentTag::Activity:
        case ComponentTag::ActivityAlias:
          emplace_classes(activities_, tag_info);
          break;
        case ComponentTag::Service:
          emplace_classes(services_, tag_info);
          break;
        case ComponentTag::Receiver:
          emplace_classes(receivers_, tag_info);
          break;
        case ComponentTag::Provider:
          emplace_classes(providers_, tag_info);
          break;
      }
    }
  } catch (const std::exception& e) {
    // Redex may assert, or throw `std::runtime_error` if the file is missing.
    auto error = fmt::format("Manifest could not be parsed: {}", e.what());
    ERROR(1, error);
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

      if (has_privacy_decision_in_class(clazz)) {
        std::lock_guard<std::mutex> lock(mutex);
        privacy_decision_classes_.emplace(clazz->str());
      }
    });
  }
}

void ClassProperties::emplace_classes(
    std::unordered_map<std::string_view, ExportedKind>& map,
    const ComponentTagInfo& tag_info) {
  auto dex_class = redex::get_class(strings_[tag_info.classname]);

  if (tag_info.is_exported == BooleanXMLAttribute::True ||
      (tag_info.is_exported == BooleanXMLAttribute::Undefined &&
       tag_info.has_intent_filters)) {
    if (tag_info.permission.empty()) {
      map.emplace(strings_[tag_info.classname], ExportedKind::Exported);
      if (dex_class) {
        auto parent_classes =
            generator::get_custom_parents_from_class(dex_class);
        for (const auto& klass : parent_classes) {
          map.emplace(klass, ExportedKind::Exported);
        }
      }
    } else {
      map.emplace(
          strings_[tag_info.classname], ExportedKind::ExportedWithPermission);
    }
  } else {
    map.emplace(strings_[tag_info.classname], ExportedKind::Unexported);
  }
}

FeatureSet ClassProperties::get_manifest_features(
    std::string_view class_name,
    const std::unordered_map<std::string_view, ExportedKind>& component_set)
    const {
  FeatureSet features;
  {
    auto it = component_set.find(class_name);
    if (it != component_set.end()) {
      if (it->second == ExportedKind::Exported) {
        features.add(feature_factory_.get("via-caller-exported"));
      } else if (it->second == ExportedKind::ExportedWithPermission) {
        features.add(feature_factory_.get("via-caller-exported"));
        features.add(feature_factory_.get("via-caller-permission"));
      } else if (it->second == ExportedKind::Unexported) {
        features.add(feature_factory_.get("via-caller-unexported"));
      }
      return features;
    }
  }
  {
    auto outer_class = strip_inner_class(class_name);
    auto it = component_set.find(outer_class);
    if (it != component_set.end()) {
      if (it->second == ExportedKind::Exported) {
        features.add(feature_factory_.get("via-caller-exported"));
      } else if (it->second == ExportedKind::ExportedWithPermission) {
        features.add(feature_factory_.get("via-caller-exported"));
        features.add(feature_factory_.get("via-caller-permission"));
      } else if (it->second == ExportedKind::Unexported) {
        features.add(feature_factory_.get("via-caller-unexported"));
      }
      return features;
    }
  }

  return features;
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

bool ClassProperties::has_privacy_decision(const Method* method) const {
  return (method->dex_method()->get_anno_set() &&
          has_privacy_decision_annotation(
              method->dex_method()->get_anno_set()->get_annotations())) ||
      privacy_decision_classes_.count(method->get_class()->str()) > 0;
}

FeatureMayAlwaysSet ClassProperties::propagate_features(
    const Method* caller,
    const Method* /*callee*/,
    const FeatureFactory& feature_factory) const {
  FeatureSet features;

  if (has_privacy_decision(caller)) {
    features.add(feature_factory.get("via-privacy-decision"));
  }

  return FeatureMayAlwaysSet::make_always(features);
}

FeatureMayAlwaysSet ClassProperties::issue_features(
    const Method* method,
    std::unordered_set<const Kind*> kinds) const {
  FeatureSet features;
  auto clazz = method->get_class()->str();

  for (const auto* kind : kinds) {
    const auto* named_kind = kind->as<NamedKind>();
    if (named_kind == nullptr ||
        !is_manifest_relevant_kind(named_kind->name())) {
      continue;
    }

    auto kind_features =
        get_class_features(clazz, named_kind, /* via_dependency */ false);

    if (!kind_features.contains(feature_factory_.get("via-caller-exported")) &&
        !kind_features.contains(
            feature_factory_.get("via-caller-unexported"))) {
      kind_features.join_with(
          compute_transitive_class_features(method, named_kind));
    }
    features.join_with(kind_features);
  }

  return FeatureMayAlwaysSet::make_always(features);
}

FeatureSet ClassProperties::get_class_features(
    std::string_view clazz,
    const NamedKind* kind,
    bool via_dependency,
    size_t dependency_depth) const {
  FeatureSet features;

  if (kind->name() == "ActivityUserInput" ||
      kind->name() == "ActivityLifecycle") {
    features.join_with(get_manifest_features(clazz, activities_));
  }

  if (kind->name() == "ReceiverUserInput") {
    features.join_with(get_manifest_features(clazz, receivers_));
  }

  if (kind->name() == "ServiceUserInput") {
    features.join_with(get_manifest_features(clazz, services_));
  }

  if (kind->name() == "ServiceAIDLUserInput") {
    if (const auto* dex_class = redex::get_class(clazz)) {
      const auto* service_class = get_service_from_stub(dex_class);
      if (service_class != nullptr) {
        features.join_with(
            get_manifest_features(service_class->str(), services_));
      } else {
        features.join_with(get_manifest_features(clazz, services_));
      }
    }
  }

  if (kind->name() == "ProviderUserInput") {
    features.join_with(get_manifest_features(clazz, providers_));
  }

  if (has_inline_permissions(clazz)) {
    features.add(feature_factory_.get("via-permission-check-in-class"));
  }

  // `via-public-dfa-scheme` feature only applies within the same class.
  if (!via_dependency && is_dfa_public(clazz)) {
    features.add(feature_factory_.get("via-public-dfa-scheme"));
  }
  if (via_dependency) {
    features.add(feature_factory_.get("via-dependency-graph"));
    features.add(feature_factory_.get(fmt::format("via-class:{}", clazz)));
    if (dependency_depth > 5) {
      features.add(feature_factory_.get("via-dependency-graph-depth-above-5"));
    }
  }

  return features;
}

namespace {

struct QueueItem {
  const Method* method;
  size_t depth;
};

} // namespace

DexClass* MT_NULLABLE
ClassProperties::get_service_from_stub(const DexClass* clazz) {
  auto constructors = clazz->get_ctors();
  if (constructors.size() != 1) {
    return nullptr;
  }
  const auto* method_prototype = constructors[0]->get_proto();
  const auto* method_arguments = method_prototype->get_args();

  if (method_arguments->size() == 0) {
    return nullptr;
  }

  auto* first_argument = type_class(method_arguments->at(0));
  auto argument_parents = generator::get_parents_from_class(
      first_argument, /* include_interfaces */ true);
  if (argument_parents.find("Landroid/app/Service;") ==
      argument_parents.end()) {
    return nullptr;
  }
  return first_argument;
}

FeatureSet ClassProperties::compute_transitive_class_features(
    const Method* callee,
    const NamedKind* kind) const {
  // Check cache
  if (const auto* target_method = via_dependencies_.get(callee, nullptr)) {
    return get_class_features(
        target_method->get_class()->str(), kind, /* via_dependency */ true);
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

    const auto& features = get_class_features(class_name, kind, false);

    if (features.contains(feature_factory_.get("via-caller-exported"))) {
      target = item;
      break;
    }

    if (target.method == nullptr &&
        features.contains(feature_factory_.get("via-caller-unexported"))) {
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
        kind,
        /* via_dependency */ true,
        /* depedency_length */ depth);
  }

  return FeatureSet();
}

} // namespace marianatrench

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
      kind == "ServiceAIDLUserInput" || kind == "ProviderUserInput" ||
      kind == "ActivityExitNode" || kind == "ProviderExitNode" ||
      kind == "ServiceAIDLExitNode");
}

bool is_class_accessible_via_dfa(const DexClass* clazz) {
  if (!clazz->get_anno_set()) {
    return false;
  }

  auto dfa_annotation_type =
      marianatrench::constants::get_dfa_annotation_type();
  for (const auto& annotation : clazz->get_anno_set()->get_annotations()) {
    if (!annotation->type() ||
        annotation->type()->str() != dfa_annotation_type) {
      continue;
    }

    auto public_scope = marianatrench::constants::get_public_access_scope();
    for (const DexAnnotationElement& element : annotation->anno_elems()) {
      if (element.string->str() == "enforceScope" &&
          element.encoded_value->as_value() == 0) {
        return true;
      }

      if (element.string->str() == "accessScope" &&
          element.encoded_value->show() == public_scope) {
        return true;
      }
    }
  }
  return false;
}

bool has_permission_check(const DexClass* clazz) {
  static const std::vector<std::string_view> k_permission_check_patterns = {
      "TrustedCaller",
      "AbilityIPCPermissionManager",
      "CallerInfoHelper",
      "AppUpdateRequestIntentVerifier",
      "TrustManager",
      "CallingIpcPermissionManager",
      "IntentHandlerScope;.SAME_KEY",
      "IntentHandlerScope;.FAMILY"};

  auto methods = clazz->get_all_methods();
  for (DexMethod* method : methods) {
    const auto& code = method->get_code();
    if (!code) {
      continue;
    }
    const cfg::ControlFlowGraph& cfg = code->cfg();
    for (const auto* block : cfg.blocks()) {
      auto show_block = show(block);

      auto matched_string = std::find_if(
          k_permission_check_patterns.begin(),
          k_permission_check_patterns.end(),
          [&show_block](const std::string_view pattern) {
            return boost::contains(show_block, pattern);
          });

      if (matched_string != k_permission_check_patterns.end()) {
        LOG(4, "Permission check found in block: {}", *matched_string);
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
          update_classes(activities_, tag_info);
          break;
        case ComponentTag::Service:
          update_classes(services_, tag_info);
          break;
        case ComponentTag::Receiver:
          update_classes(receivers_, tag_info);
          break;
        case ComponentTag::Provider:
          update_classes(providers_, tag_info);
          break;
      }
    }
  } catch (const std::exception& e) {
    // Redex may assert, or throw `std::runtime_error` if the file is missing.
    auto error = fmt::format("Manifest could not be parsed: {}", e.what());
    ERROR(1, error);
    EventLogger::log_event("manifest_error", error, /* verbosity_level */ 1);
  }

  std::mutex mutex;
  for (auto& scope : DexStoreClassesIterator(stores)) {
    walk::parallel::classes(scope, [&mutex, this](DexClass* clazz) {
      if (is_class_accessible_via_dfa(clazz)) {
        std::lock_guard<std::mutex> lock(mutex);
        dfa_public_scheme_classes_.emplace(clazz->str());
      }

      if (has_permission_check(clazz)) {
        std::lock_guard<std::mutex> lock(mutex);
        inline_permission_classes_.emplace(clazz->str());
      }
    });
  }
}

void ClassProperties::update_classes(
    std::unordered_map<std::string_view, ExportedKind>& map,
    const ComponentTagInfo& tag_info) {
  if (tag_info.is_exported == BooleanXMLAttribute::True ||
      (tag_info.is_exported == BooleanXMLAttribute::Undefined &&
       tag_info.has_intent_filters)) {
    if (tag_info.permission.empty()) {
      map.insert_or_assign(
          strings_[tag_info.classname], ExportedKind::Exported);
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

FeatureMayAlwaysSet ClassProperties::issue_features(
    const Method* method,
    const std::unordered_set<const Kind*>& kinds,
    const Heuristics& heuristics) const {
  FeatureSet features;
  auto clazz = method->get_class()->str();

  for (const auto* kind : kinds) {
    auto normalized_kind = kind->discard_transforms();
    const auto* named_kind = normalized_kind->as<NamedKind>();
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
          compute_transitive_class_features(method, named_kind, heuristics));
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
  auto normalized_kind = kind->discard_transforms();
  const auto* named_kind = normalized_kind->as<NamedKind>();

  if (named_kind != nullptr) {
    if (named_kind->name() == "ActivityUserInput" ||
        named_kind->name() == "ActivityLifecycle" ||
        named_kind->name() == "ActivityExitNode") {
      features.join_with(get_manifest_features(clazz, activities_));
    }

    if (named_kind->name() == "ReceiverUserInput") {
      features.join_with(get_manifest_features(clazz, receivers_));
    }

    if (named_kind->name() == "ServiceUserInput") {
      features.join_with(get_manifest_features(clazz, services_));
    }

    if (named_kind->name() == "ServiceAIDLUserInput" ||
        named_kind->name() == "ServiceAIDLExitNode") {
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

    if (named_kind->name() == "ProviderUserInput" ||
        named_kind->name() == "ProviderExitNode") {
      features.join_with(get_manifest_features(clazz, providers_));
    }
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
    const NamedKind* kind,
    const Heuristics& heuristics) const {
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

    if (depth == heuristics.max_depth_class_properties()) {
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

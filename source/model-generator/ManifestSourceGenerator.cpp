/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <algorithm>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include <RedexResources.h>

#include <mariana-trench/CallGraph.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/ManifestSourceGenerator.h>

namespace marianatrench {

namespace {

static const std::unordered_set<std::string> permission_base_class_prefixes = {
    "Lcom/facebook/secure/content/FbPermissions",
    "Lcom/facebook/secure/content/Secure",
    "Lcom/oculus/content/OculusFbPermissions",
    "Lcom/facebook/secure/service/FbPermissions",
    "Lcom/facebook/secure/ktx/service/FbPermissions",
    "Lcom/oculus/security/basecomponent/OculusFbPermission",
    "Lcom/facebook/secure/content/FbPermissions",
    "Lcom/facebook/secure/receiver/Family",
    "Lcom/facebook/secure/receiver/Internal"};

bool has_secure_base_class(const DexClass* dex_class) {
  auto parent_classes = generator::get_custom_parents_from_class(dex_class);
  for (const auto& parent_class : parent_classes) {
    for (const auto& class_prefix : permission_base_class_prefixes) {
      if (boost::starts_with(parent_class, class_prefix)) {
        return true;
      }
    }
  }
  return false;
}
} // namespace

ManifestSourceGenerator::ManifestSourceGenerator(Context& context)
    : ModelGenerator("manifest_source_generator", context) {
  mt_assert_log(
      context.call_graph != nullptr,
      "Manifest source generator requires CallGraph to be built.");
  mt_assert(context.options != nullptr);
  resources_directory_ = context.options->apk_directory();
}

std::vector<Model> ManifestSourceGenerator::emit_method_models(
    const Methods& methods) {
  auto android_resources = create_resource_reader(resources_directory_);
  if (!android_resources) {
    WARNING(
        1, "No android resources found. Skipping manifest source generator...");
    return {};
  }

  const auto manifest_class_info = android_resources->get_manifest_class_info();

  ConcurrentSet<const DexClass*> exported_classes;
  for (const ComponentTagInfo& tag_info : manifest_class_info.component_tags) {
    if (!tag_info.permission.empty()) {
      continue;
    }
    if (tag_info.is_exported == BooleanXMLAttribute::True ||
        (tag_info.is_exported == BooleanXMLAttribute::Undefined &&
         tag_info.has_intent_filters)) {
      const auto* dex_klass = type_class(redex::get_type(tag_info.classname));
      if (dex_klass == nullptr) {
        LOG(5, "Could not find dex type for classname: {}", tag_info.classname);
      } else if (!has_secure_base_class(dex_klass)) {
        exported_classes.emplace(dex_klass);
      }
    }
  }

  ConcurrentSet<const DexClass*> nested_exported_classes;
  for (auto& scope : DexStoreClassesIterator(context_.stores)) {
    walk::parallel::classes(
        scope, [&exported_classes, &nested_exported_classes](DexClass* clazz) {
          for (const DexClass* exported_class :
               UnorderedIterable(exported_classes)) {
            auto dex_klass_prefix = exported_class->get_name()->str_copy();
            dex_klass_prefix.erase(dex_klass_prefix.length() - 1);

            if (boost::starts_with(show(clazz), dex_klass_prefix)) {
              nested_exported_classes.emplace(clazz);
            }
          }
        });
  }

  insert_unordered_iterable(exported_classes, nested_exported_classes);

  std::vector<Model> models;
  for (const auto* dex_klass : UnorderedIterable(exported_classes)) {
    // Mark all public and protected methods in the class as exported.
    for (const auto* dex_callee : dex_klass->get_all_methods()) {
      if (dex_callee == nullptr) {
        continue;
      }

      if (dex_callee->get_access() & DexAccessFlags::ACC_PRIVATE) {
        continue;
      }

      const auto* callee = methods.get(dex_callee);
      if (callee == nullptr) {
        continue;
      }

      Model model(callee, context_);
      model.add_call_effect_source(
          AccessPath(Root(Root::Kind::CallEffectExploitability)),
          generator::source(
              context_,
              /* kind */ "ExportedComponent"),
          *context_.heuristics);
      models.push_back(model);
    }
  }
  return models;
}

} // namespace marianatrench

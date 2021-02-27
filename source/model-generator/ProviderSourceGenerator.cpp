/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <re2/re2.h>

#include <Walkers.h>

#include <mariana-trench/Log.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/ProviderSourceGenerator.h>

namespace marianatrench {

namespace {

std::unordered_set<std::string> provider_regex_strings = {
    ".*;\\.(doQ|q)uery:\\(Landroid/net/Uri;\\[Ljava/lang/String;.*\\)Landroid/database/Cursor;",
    ".*;\\.(doB|b)ulkInsert:\\(Landroid/net/Uri;\\[Landroid/content/ContentValues;\\)I",
    ".*;\\.(doD|d)elete:\\(Landroid/net/Uri;.*\\)I",
    ".*;\\.(doI|i)nsert:\\(Landroid/net/Uri;Landroid/content/ContentValues;.*\\)Landroid/net/Uri;",
    ".*;\\.(doU|u)pdate:\\(Landroid/net/Uri;Landroid/content/ContentValues;.*\\)I",
    ".*;\\.(doA|a)pplyBatch:\\(.*Ljava/util/ArrayList;\\)\\[Landroid/content/ContentProviderResult;",
    ".*;\\.(doC|c)all:\\(Ljava/lang/String;Ljava/lang/String;Landroid/os/Bundle;\\)Landroid/os/Bundle;",
    ".*;\\.(doO|o)penAssetFile:\\(Landroid/net/Uri;Ljava/lang/String;.*\\)Landroid/content/res/AssetFileDescriptor;",
    ".*;\\.(doO|o)penFile:\\(Landroid/net/Uri;Ljava/lang/String;.*\\)Landroid/os/ParcelFileDescriptor;",
    ".*;\\.(doO|o)penPipeHelper:\\(Landroid/net/Uri;Ljava/lang/String;.*\\)Landroid/os/ParcelFileDescriptor;",
    ".*;\\.(doO|o)penTypedAssetFile:\\(Landroid/net/Uri;Ljava/lang/String;.*\\)Landroid/content/res/AssetFileDescriptor;",
    ".*;\\.(doQ|q)uery:\\(Landroid/net/Uri;\\[Ljava/lang/String;.*\\)Landroid/database/Cursor;"};

std::unordered_set<std::string> permission_method_suffixes = {
    ".onCheckPermissions:()Z",
    ".getFbPermission:()Ljava/lang/String;",
    ".getWhitelistedPackages:()Lcom/google/common/collect/ImmutableSet;"};

std::unordered_set<std::string> permission_base_class_prefixes = {
    "Lcom/facebook/secure/content/FbPermissions",
    "Lcom/facebook/secure/content/Secure"};

bool has_inline_permissions(DexClass* dex_class) {
  for (const auto& method_suffix : permission_method_suffixes) {
    if (redex::get_method(dex_class->str() + method_suffix)) {
      return true;
    }
  }

  std::unordered_set<std::string> parent_classes =
      generator::get_custom_parents_from_class(dex_class);
  for (const auto& parent_class : parent_classes) {
    for (const auto& class_prefix : permission_base_class_prefixes) {
      if (boost::starts_with(parent_class, class_prefix)) {
        return true;
      }
    }
  }
  return false;
}

Model source_all_parameters(
    const Method* method,
    bool has_permissions,
    Context& context) {
  std::vector<std::string> features;
  auto model = Model(method, context);
  model.add_mode(Model::Mode::NoJoinVirtualOverrides, context);
  if (has_permissions) {
    features.push_back("via-caller-permission");
  }
  for (const auto& argument : generator::get_argument_types(method)) {
    model.add_parameter_source(
        AccessPath(Root(Root::Kind::Argument, argument.first)),
        generator::source(
            context,
            method,
            /* kind */ "ProviderUserInput",
            features));
  }
  return model;
}

} // namespace

std::vector<Model> ProviderSourceGenerator::run(const Methods& methods) {
  std::unordered_set<std::string> manifest_providers = {};
  try {
    const auto manifest_class_info = get_manifest_class_info(
        options_.apk_directory() + "/AndroidManifest.xml");

    for (const auto& tag_info : manifest_class_info.component_tags) {
      if (tag_info.tag == ComponentTag::Provider) {
        auto* dex_class = redex::get_class(tag_info.classname);
        if (dex_class) {
          std::unordered_set<std::string> parent_classes =
              generator::get_custom_parents_from_class(dex_class);
          for (const auto& parent_class : parent_classes) {
            manifest_providers.emplace(
                generator::get_outer_class(parent_class));
          }
        }
        manifest_providers.emplace(
            generator::get_outer_class(tag_info.classname));
      }
    }
  } catch (const std::exception& e) {
    // Redex may assert, or throw `std::runtime_error` if the file is missing.
    ERROR(2, "Manifest could not be parsed: {}", e.what());
    return {};
  }

  std::vector<std::unique_ptr<re2::RE2>> provider_regexes;
  for (const auto& regex_string : provider_regex_strings) {
    provider_regexes.push_back(std::make_unique<re2::RE2>(regex_string));
  }

  std::mutex mutex;
  std::vector<Model> models;
  std::unordered_map<DexClass*, bool> permission_providers = {};
  auto queue = sparta::work_queue<const Method*>([&](const Method* method) {
    std::string signature = method->show();
    const auto outer_class = generator::get_outer_class(signature);
    if (manifest_providers.count(outer_class)) {
      for (const auto& regex : provider_regexes) {
        if (re2::RE2::FullMatch(signature, *regex)) {
          bool has_permissions = false;
          std::lock_guard<std::mutex> lock(mutex);
          auto* dex_class = type_class(method->get_class());

          if (dex_class) {
            auto found = permission_providers.find(dex_class);
            if (found != permission_providers.end()) {
              has_permissions = found->second;
            } else {
              has_permissions = has_inline_permissions(dex_class);
              permission_providers.emplace(dex_class, has_permissions);
            }
          }
          models.push_back(
              source_all_parameters(method, has_permissions, context_));
          break;
        }
      }
    }
  });
  for (const auto* method : methods) {
    queue.add_item(method);
  }
  queue.run_all();
  return models;
}

} // namespace marianatrench

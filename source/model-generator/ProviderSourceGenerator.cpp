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

Model source_all_parameters(const Method* method, Context& context) {
  auto model = Model(method, context);
  model.add_mode(Model::Mode::NoJoinVirtualOverrides);
  for (const auto& argument : generator::get_argument_types(method)) {
    model.add_parameter_source(
        AccessPath(Root(Root::Kind::Argument, argument.first)),
        generator::source(
            context,
            method,
            /* kind */ "ProviderUserInput"));
  }
  return model;
}

} // namespace

std::vector<Model> ProviderSourceGenerator::run(const DexStoresVector& stores) {
  std::vector<Model> models;
  std::unordered_set<std::string> manifest_providers = {};
  std::vector<std::unique_ptr<re2::RE2>> provider_regexes;

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

  for (const auto& regex_string : provider_regex_strings) {
    provider_regexes.push_back(std::make_unique<re2::RE2>(regex_string));
  }

  std::mutex mutex;
  for (auto& scope : DexStoreClassesIterator(stores)) {
    walk::parallel::methods(scope, [&](DexMethod* dex_method) {
      const auto* method = methods_.get(dex_method);
      std::string signature = show(method);

      if (manifest_providers.find(generator::get_outer_class(signature)) !=
          manifest_providers.end()) {
        for (const auto& regex : provider_regexes) {
          if (re2::RE2::FullMatch(signature, *regex)) {
            std::lock_guard<std::mutex> lock(mutex);
            models.push_back(source_all_parameters(method, context_));
            break;
          }
        }
      }
    });
  }
  return models;
}

} // namespace marianatrench

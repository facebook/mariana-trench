/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <re2/re2.h>

#include <Walkers.h>

#include <mariana-trench/Log.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/ContentProviderGenerator.h>

namespace marianatrench {

namespace {

std::unordered_set<std::string> provider_regex_strings = {
    ".*;\\.query:\\(Landroid/net/Uri;\\[Ljava/lang/String;.*\\)Landroid/database/Cursor;",
    ".*;\\.bulkInsert:\\(Landroid/net/Uri;\\[Landroid/content/ContentValues;\\)I",
    ".*;\\.delete:\\(Landroid/net/Uri;.*\\)I",
    ".*;\\.insert:\\(Landroid/net/Uri;Landroid/content/ContentValues;.*\\)Landroid/net/Uri;",
    ".*;\\.update:\\(Landroid/net/Uri;Landroid/content/ContentValues;.*\\)I",
    ".*;\\.applyBatch:\\(.*Ljava/util/ArrayList;\\)\\[Landroid/content/ContentProviderResult;",
    ".*;\\.call:\\(Ljava/lang/String;Ljava/lang/String;Landroid/os/Bundle;\\)Landroid/os/Bundle;",
    ".*;\\.openAssetFile:\\(Landroid/net/Uri;Ljava/lang/String;.*\\)Landroid/content/res/AssetFileDescriptor;",
    ".*;\\.openFile:\\(Landroid/net/Uri;Ljava/lang/String;.*\\)Landroid/os/ParcelFileDescriptor;",
    ".*;\\.openPipeHelper:\\(Landroid/net/Uri;Ljava/lang/String;.*\\)Landroid/os/ParcelFileDescriptor;",
    ".*;\\.openTypedAssetFile:\\(Landroid/net/Uri;Ljava/lang/String;.*\\)Landroid/content/res/AssetFileDescriptor;"};

Model create_model(
    const Method* method,
    const ModelGeneratorName* generator_name,
    Context& context) {
  auto model = Model(method, context);
  model.add_mode(Model::Mode::NoJoinVirtualOverrides, context);
  model.add_model_generator(generator_name);
  for (const auto& argument : generator::get_argument_types(method)) {
    model.add_parameter_source(
        AccessPath(Root(Root::Kind::Argument, argument.first)),
        generator::source(
            context,
            /* kind */ "ProviderUserInput"),
        context.options->heuristics());
  }
  auto return_type = generator::get_return_type_string(method);
  if (!return_type) {
    return model;
  }
  if (!boost::equals(*return_type, "I")) {
    model.add_sink(
        AccessPath(Root(Root::Kind::Return)),
        generator::sink(
            context,
            /* kind */ "ProviderExitNode"),
        context.options->heuristics());
  }
  return model;
}

} // namespace

std::vector<Model> ContentProviderGenerator::emit_method_models(
    const Methods& methods) {
  std::unordered_set<std::string> manifest_providers = {};
  try {
    auto android_resources = create_resource_reader(options_.apk_directory());
    const auto manifest_class_info =
        android_resources->get_manifest_class_info();

    for (const auto& tag_info : manifest_class_info.component_tags) {
      if (tag_info.tag == ComponentTag::Provider) {
        auto* dex_class = redex::get_class(tag_info.classname);
        if (dex_class) {
          std::unordered_set<std::string_view> parent_classes =
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
  auto queue = sparta::work_queue<const Method*>([&](const Method* method) {
    const auto& signature = method->signature();
    const auto outer_class = generator::get_outer_class(signature);
    if (manifest_providers.count(outer_class)) {
      for (const auto& regex : provider_regexes) {
        if (re2::RE2::FullMatch(signature, *regex)) {
          std::lock_guard<std::mutex> lock(mutex);
          models.push_back(create_model(method, name_, context_));
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

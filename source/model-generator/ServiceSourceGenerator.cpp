/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <exception>
#include <mutex>

#include <boost/algorithm/string.hpp>

#include <Walkers.h>

#include <mariana-trench/Dependencies.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/ServiceSourceGenerator.h>

namespace marianatrench {

namespace {

std::unordered_set<std::string> service_methods = {
    "onBind",
    "doBind",
    "onRebind",
    "onStart",
    "onHandleIntent",
    "doHandleIntent",
    "onSecuredHandleIntent",
    "onHandleWork",
    "onStartCommand",
    "doStartCommand",
    "onTaskRemoved",
    "onUnbind",
    "onFbStartCommand",
    "handleMessage"};

Model source_first_argument(const Method* method, Context& context) {
  auto model = Model(method, context);
  model.add_mode(Model::Mode::NoJoinVirtualOverrides, context);
  model.add_parameter_source(
      AccessPath(Root(Root::Kind::Argument, 1)),
      generator::source(
          context,
          method,
          /* kind */ "ServiceUserInput"));
  return model;
}

} // namespace

std::vector<Model> ServiceSourceGenerator::run(const Methods& methods) {
  std::vector<Model> models;

  std::unordered_set<std::string> manifest_services = {};

  try {
    const auto manifest_class_info = get_manifest_class_info(
        options_.apk_directory() + "/AndroidManifest.xml");

    for (const auto& tag_info : manifest_class_info.component_tags) {
      if (tag_info.tag == ComponentTag::Service) {
        auto* dex_class = redex::get_class(tag_info.classname);
        if (dex_class) {
          std::unordered_set<std::string> parent_classes =
              generator::get_custom_parents_from_class(dex_class);
          for (const auto& parent_class : parent_classes) {
            auto manifest_class_start =
                parent_class.substr(0, parent_class.find(";", 0));
            manifest_services.emplace(manifest_class_start);
          }
        }
        auto manifest_class_start =
            tag_info.classname.substr(0, tag_info.classname.find(";", 0));
        manifest_services.emplace(manifest_class_start);
      }
    }
  } catch (const std::exception& e) {
    // Redex may assert, or throw `std::runtime_error` if the file is missing.
    ERROR(2, "Manifest could not be parsed: {}", e.what());
  }

  std::mutex mutex;
  auto queue = sparta::work_queue<const Method*>([&](const Method* method) {
    const auto method_name = generator::get_method_name(method);
    const auto argument_types = generator::get_argument_types(method);
    const auto class_name = generator::get_class_name(method);

    if (boost::starts_with(class_name, "Landroid") ||
        argument_types.size() < 1) {
      return;
    }

    if (boost::equals(
            class_name, "Lcom/facebook/secure/service/SecureService;") ||
        boost::equals(class_name, "Lcom/facebook/base/service/FbService;")) {
      return;
    }

    if (boost::equals(method_name, "handleMessage") &&
        boost::contains(class_name, "ervice") && argument_types.size() == 1) {
      auto model = source_first_argument(method, context_);
      std::lock_guard<std::mutex> lock(mutex);
      models.push_back(model);
    }

    if (service_methods.find(method_name) != service_methods.end() &&
        manifest_services.find(generator::get_outer_class(class_name)) !=
            manifest_services.end()) {
      auto model = source_first_argument(method, context_);
      std::lock_guard<std::mutex> lock(mutex);
      models.push_back(model);
    }
  });
  for (const auto* method : methods) {
    queue.add_item(method);
  }
  queue.run_all();
  return models;
}

} // namespace marianatrench

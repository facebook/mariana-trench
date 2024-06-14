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

  auto exported_classes =
      manifest_class_info.component_tags |
      boost::adaptors::filtered([](const ComponentTagInfo& tag_info) {
        return tag_info.is_exported == BooleanXMLAttribute::True ||
            (tag_info.is_exported == BooleanXMLAttribute::Undefined &&
             tag_info.has_intent_filters);
      }) |
      boost::adaptors::transformed([](const ComponentTagInfo& tag_info) {
        const auto* dex_klass = type_class(redex::get_type(tag_info.classname));
        if (dex_klass == nullptr) {
          LOG(5,
              "Could not find dex type for classname: {}",
              tag_info.classname);
        }
        return dex_klass;
      }) |
      boost::adaptors::filtered(
          [](const DexClass* dex_class) { return dex_class != nullptr; });

  std::vector<Model> models{};
  for (const auto* dex_klass : exported_classes) {
    // Currently, only Activity classes are considered.
    const auto& direct_methods = dex_klass->get_dmethods();
    auto result = std::find_if(
        direct_methods.begin(),
        direct_methods.end(),
        [](const DexMethod* dex_method) {
          return boost::ends_with(
              dex_method->str(), "activity_lifecycle_wrapper");
        });

    if (result == direct_methods.end()) {
      // No lifecycle method found.
      continue;
    }

    const auto* lifecycle_method = methods.get(*result);
    // Mark lifecycle wrapper as exported. This is required to catch the flows
    // found on the lifecycle wrapper itself.
    Model model(lifecycle_method, context_);
    model.add_call_effect_source(
        AccessPath(Root(Root::Kind::CallEffectExploitability)),
        generator::source(
            context_,
            /* kind */ "ExportedActivity"),
        context_.options->heuristics());
    models.push_back(model);

    // Mark all callees of lifecycle wrapper as exported
    for (const auto& call_target :
         context_.call_graph->callees(lifecycle_method)) {
      const auto* callee = call_target.resolved_base_callee();
      mt_assert(callee != nullptr);
      Model model(callee, context_);
      model.add_call_effect_source(
          AccessPath(Root(Root::Kind::CallEffectExploitability)),
          generator::source(
              context_,
              /* kind */ "ExportedActivity"),
          context_.options->heuristics());
      models.push_back(model);
    }
  }

  return models;
}

} // namespace marianatrench

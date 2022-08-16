/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Options.h>
#include <mariana-trench/shim-generator/ShimGeneration.h>
#include <mariana-trench/shim-generator/ShimGenerator.h>

namespace marianatrench {
namespace {

std::vector<JsonShimGenerator> get_shim_generators(
    Context& context,
    const Json::Value& shim_definitions) {
  std::vector<JsonShimGenerator> shims;

  for (const auto& shim_definition :
       JsonValidation::null_or_array(shim_definitions)) {
    std::string find_name = JsonValidation::string(shim_definition, "find");
    if (find_name == "methods") {
      std::vector<std::unique_ptr<MethodConstraint>> shim_constraints;
      for (auto constraint :
           JsonValidation::null_or_array(shim_definition, "where")) {
        shim_constraints.push_back(
            MethodConstraint::from_json(constraint, context));
      }

      shims.push_back(JsonShimGenerator(
          std::make_unique<AllOfMethodConstraint>(std::move(shim_constraints)),
          ShimTemplate::from_json(
              JsonValidation::object(shim_definition, "shim")),
          context.methods.get()));
    }
  }

  return shims;
}

} // namespace

MethodToShimMap ShimGeneration::run(Context& context) {
  std::vector<JsonShimGenerator> all_shims;
  for (const auto& path : context.options->shims_paths()) {
    LOG(1, "Processing shim generator at: {}", path);
    try {
      auto shim_generators =
          get_shim_generators(context, JsonValidation::parse_json_file(path));

      all_shims.insert(
          all_shims.end(),
          std::make_move_iterator(shim_generators.begin()),
          std::make_move_iterator(shim_generators.end()));
    } catch (const JsonValidationError& exception) {
      WARNING(
          3,
          "Unable to parse shim generator at `{}`: {}",
          path,
          exception.what());
    }
  }

  LOG(1, "Create method mappings");
  // TODO: This is duplicate from ModelGeneration::run_optimized()
  // Consider sharing.
  std::unique_ptr<MethodMappings> method_mappings =
      std::make_unique<MethodMappings>(*context.methods);

  // Run the shim generator
  LOG(1, "Running {} shim generators", all_shims.size());
  MethodToShimMap method_shims;
  for (auto& item : all_shims) {
    auto shims =
        item.emit_method_shims(context.methods.get(), *method_mappings);
    method_shims.merge(shims);
    for (const auto& shim : shims) {
      WARNING(
          1,
          "Shim for {} already exist. Not adding: {}",
          shim.first->show(),
          shim.second);
    }
  }

  return method_shims;
}

} // namespace marianatrench

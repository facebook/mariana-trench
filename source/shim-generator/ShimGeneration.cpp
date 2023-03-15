/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <iterator>

#include <boost/algorithm/string/join.hpp>

#include <mariana-trench/EventLogger.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/shim-generator/ShimGeneration.h>
#include <mariana-trench/shim-generator/ShimGenerator.h>
#include <mariana-trench/shim-generator/Shims.h>

namespace marianatrench {
namespace {

std::vector<ShimGenerator> get_shim_generators(
    Context& context,
    const Json::Value& shim_definitions) {
  std::vector<ShimGenerator> shims;

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

      shims.push_back(ShimGenerator(
          std::make_unique<AllOfMethodConstraint>(std::move(shim_constraints)),
          ShimTemplate::from_json(
              JsonValidation::object(shim_definition, "shim")),
          context.methods.get()));
    } else {
      auto error =
          fmt::format("Shim models for `{}` are not supported.", find_name);
      ERROR(1, error);
      EventLogger::log_event("shim_generator_error", error);
    }
  }

  return shims;
}

} // namespace

ShimGeneratorError::ShimGeneratorError(const std::string& message)
    : std::invalid_argument(message) {}

Shims ShimGeneration::run(
    Context& context,
    const MethodMappings& method_mappings) {
  std::vector<ShimGenerator> all_shims;
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
      auto error = fmt::format(
          "Unable to parse shim generator at `{}`: {}", path, exception.what());
      WARNING(3, error);
      EventLogger::log_event("shim_generator_error", error);
    }
  }

  // Run the shim generator
  Shims method_shims(all_shims.size());
  std::size_t iteration = 0;

  for (auto& item : all_shims) {
    LOG(1, "Running shim generator ({}/{})", ++iteration, all_shims.size());

    auto duplicate_shims = item.emit_method_shims(
        method_shims, context.methods.get(), method_mappings);

    if (!duplicate_shims.empty()) {
      std::vector<std::string> errors{};
      std::transform(
          duplicate_shims.cbegin(),
          duplicate_shims.cend(),
          std::back_inserter(errors),
          [](const auto& shim) { return shim.method()->show(); });

      throw ShimGeneratorError(fmt::format(
          "Duplicate shim defined for: {}", boost::join(errors, ", ")));
    }
  }

  return method_shims;
}

} // namespace marianatrench

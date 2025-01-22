/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <mariana-trench/model-generator/ModelGenerator.h>
#include <mariana-trench/model-generator/TaintInTaintOutGenerator.h>

namespace marianatrench {

namespace {
std::unordered_set<std::string_view> numeric_types = {"J", "F", "D", "I", "S"};

std::unordered_set<std::string_view> uri_types = {
    "Landroid/net/Uri;",
    "Ljava/net/URI;"};
} // namespace

std::vector<Model> TaintInTaintOutGenerator::visit_method(
    const Method* method) const {
  if (method->get_code() || method->returns_void()) {
    return {};
  }

  if (preloaded_models_ && preloaded_models_->has_model(method)) {
    // Do not overwrite preloaded models with taint-in-taint-out.
    return {};
  }

  if (method->is_abstract() || method->is_interface()) {
    const auto& overrides = context_.overrides->get(method);
    if (!overrides.empty() &&
        overrides.size() < context_.heuristics->join_override_threshold() &&
        !context_.overrides->has_obscure_override_for(method)) {
      return {};
    }
  }

  const auto method_signature = method->show();
  if (boost::ends_with(method_signature, ".size:()I") ||
      boost::ends_with(method_signature, ".hashCode:()I") ||
      boost::starts_with(method_signature, "Ljava/lang/Object;.getClass:")) {
    return {};
  }

  auto model = Model(method, context_);
  for (ParameterPosition parameter_position = 0;
       parameter_position < method->number_of_parameters();
       parameter_position++) {
    auto parameter_type = method->parameter_type(parameter_position);
    if (parameter_type == nullptr) {
      continue;
    }
    auto parameter_type_string = parameter_type->str();
    std::vector<std::string> feature_set{"via-obscure-taint-in-taint-out"};
    auto return_type = generator::get_return_type_string(method);
    if (return_type != std::nullopt) {
      if (numeric_types.find(*return_type) != numeric_types.end() ||
          numeric_types.find(parameter_type_string) != numeric_types.end()) {
        feature_set.emplace_back("cast:numeric");
      }
      if (*return_type == "Z" || parameter_type_string == "Z") {
        feature_set.emplace_back("cast:boolean");
      }
      if (uri_types.find(*return_type) != uri_types.end() ||
          uri_types.find(parameter_type_string) != uri_types.end()) {
        feature_set.emplace_back("cast:uri");
      }
    }

    generator::add_propagation_to_return(
        context_,
        model,
        parameter_position,
        CollapseDepth::collapse(),
        feature_set);
  }

  return {model};
}

} // namespace marianatrench

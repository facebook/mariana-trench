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
}

std::vector<Model> TaintInTaintOutGenerator::visit_method(
    const Method* method) const {
  if (method->get_code() || method->returns_void()) {
    return {};
  }

  if (method->is_abstract() || method->is_interface()) {
    const auto& overrides = context_.overrides->get(method);
    if (!overrides.empty() &&
        overrides.size() < Heuristics::kJoinOverrideThreshold &&
        !context_.overrides->has_obscure_override_for(method)) {
      return {};
    }
  }

  const auto class_name = generator::get_class_name(method);

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
    // This logic sanitizes Argument(0) -> Return on specific classes
    if (parameter_position == 0 && !method->is_static() &&
        (boost::ends_with(class_name, "Context;") ||
         boost::ends_with(class_name, "ContextThemeWrapper;") ||
         boost::ends_with(class_name, "ContextWrapper;") ||
         boost::ends_with(class_name, "Activity;") ||
         boost::ends_with(class_name, "Service;") ||
         boost::ends_with(class_name, "WebView;") ||
         boost::ends_with(class_name, "Fragment;") ||
         boost::ends_with(class_name, "WebViewClient;") ||
         boost::ends_with(class_name, "ContentProvider;") ||
         boost::ends_with(class_name, "BroadcastReceiver;"))) {
      continue;
    }

    auto parameter_type = method->parameter_type(parameter_position);
    if (parameter_type == nullptr) {
      continue;
    }
    auto parameter_type_string = parameter_type->str();
    if (parameter_type_string == "Landroid/content/Context;") {
      continue;
    }

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

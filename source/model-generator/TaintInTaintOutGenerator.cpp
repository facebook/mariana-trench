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

  // This logic sanitizes Argument(0) -> Return on specifc classes
  const auto class_name = generator::get_class_name(method);
  if (!method->is_static() &&
      (boost::contains(class_name, "Context;") ||
       boost::contains(class_name, "ContextThemeWrapper;") ||
       boost::contains(class_name, "ContextWrapper;") ||
       boost::contains(class_name, "Activity;") ||
       boost::contains(class_name, "Service;") ||
       boost::contains(class_name, "WebView;") ||
       boost::contains(class_name, "Fragment;") ||
       boost::contains(class_name, "WebViewClient;") ||
       boost::contains(class_name, "ContentProvider;") ||
       boost::contains(class_name, "BroadcastReceiver;"))) {
    auto model = Model(method, context_);
    for (ParameterPosition parameter_position = 1;
         parameter_position < method->number_of_parameters();
         parameter_position++) {
      generator::add_propagation_to_return(
          context_,
          model,
          parameter_position,
          CollapseDepth::collapse(),
          {"via-obscure-taint-in-taint-out"});
    }
    return {model};
  }

  const auto method_signature = show(method);
  if (boost::ends_with(method_signature, ".size:()I") ||
      boost::ends_with(method_signature, ".hashCode:()I") ||
      boost::starts_with(method_signature, "Ljava/lang/Object;.getClass:")) {
    return {};
  }

  return {Model(method, context_, Model::Mode::TaintInTaintOut)};
}

} // namespace marianatrench

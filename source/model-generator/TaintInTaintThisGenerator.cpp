/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string.hpp>

#include <mariana-trench/model-generator/ModelGenerator.h>
#include <mariana-trench/model-generator/TaintInTaintThisGenerator.h>

namespace marianatrench {

std::vector<Model> TaintInTaintThisGenerator::visit_method(
    const Method* method) const {
  if (method->get_code() || method->is_static()) {
    return {};
  }

  const auto class_name = generator::get_class_name(method);
  if (boost::contains(class_name, "Activity;") ||
      boost::contains(class_name, "Service;") ||
      boost::contains(class_name, "WebView;") ||
      boost::contains(class_name, "Fragment;") ||
      boost::contains(class_name, "WebViewClient;") ||
      boost::contains(class_name, "ContentProvider;") ||
      boost::contains(class_name, "BroadcastReceiver;")) {
    return {};
  }

  const auto method_name = generator::get_method_name(method);
  auto model = Model(method, context_, Model::Mode::TaintInTaintThis);
  if (!boost::equals(method_name, "<init>") &&
      !boost::starts_with(method_name, "add") &&
      !boost::starts_with(method_name, "set") &&
      !boost::starts_with(method_name, "put") &&
      !boost::starts_with(method_name, "append") &&
      !boost::starts_with(method_name, "unmarshall") &&
      !boost::starts_with(method_name, "write")) {
    for (ParameterPosition parameter_position = 1;
         parameter_position < method->number_of_parameters();
         parameter_position++) {
      generator::add_propagation_to_self(
          context_,
          model,
          parameter_position,
          {"via-obscure-taint-in-taint-this"});
    }
  }

  return {model};
}

} // namespace marianatrench

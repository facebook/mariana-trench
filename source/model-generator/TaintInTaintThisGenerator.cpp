/*
 * Copyright (c) Facebook, Inc. and its affiliates.
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
  if (!boost::contains(class_name, "Activity;") &&
      !boost::contains(class_name, "Service;") &&
      !boost::contains(class_name, "WebView;") &&
      !boost::contains(class_name, "Fragment;") &&
      !boost::contains(class_name, "WebViewClient;") &&
      !boost::contains(class_name, "ContentProvider;") &&
      !boost::contains(class_name, "BroadcastReceiver;")) {
    return {Model(method, context_, Model::Mode::TaintInTaintThis)};
  }

  return {};
}

} // namespace marianatrench

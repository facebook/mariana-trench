/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string/predicate.hpp>
#include <fmt/format.h>

#include <mariana-trench/model-generator/StructuredLoggerSinkGenerator.h>

namespace marianatrench {

std::vector<Model> StructuredLoggerSinkGenerator::visit_method(
    const Method* method) const {
  const auto class_name = generator::get_class_name(method);
  const auto method_name = generator::get_method_name(method);

  auto argument_size = generator::get_argument_types(method).size();
  if (argument_size == 0) {
    return {};
  }

  // Look for logging methods in FB USL generated classes.
  if (boost::starts_with(
          class_name, "Lcom/facebook/analytics/structuredlogger/events") &&
      !boost::algorithm::ends_with(class_name, "Impl;") &&
      boost::starts_with(method_name, "set")) {
    auto model = Model(method, context_);
    model.add_mode(Model::Mode::SkipAnalysis, context_);
    model.add_sink(
        AccessPath(Root(Root::Kind::Argument, 1)),
        generator::sink(
            context_,
            method,
            /* kind */ "Logging",
            /* features */ {},
            /* callee_port */ Root::Kind::Anchor));
    return {model};
  }

  // Look for IG logger methods and legacy HoneyClientEvent logging methods.
  if ((class_name == "Lcom/instagram/common/analytics/intf/AnalyticsEvent;" &&
       boost::starts_with(method_name, "addExtra")) ||
      ((boost::algorithm::ends_with(class_name, "HoneyClientEvent;") ||
        boost::algorithm::ends_with(class_name, "HoneyAnalyticsEvent;")) &&
       boost::starts_with(method_name, "addParameter"))) {
    ParameterPosition position = argument_size == 1 ? 1 : 2;
    auto model = Model(method, context_);
    model.add_mode(Model::Mode::SkipAnalysis, context_);
    model.add_sink(
        AccessPath(Root(Root::Kind::Argument, position)),
        generator::sink(
            context_,
            method,
            /* kind */ "Logging",
            /* features */ {},
            /* callee_port */ Root::Kind::Anchor));
    return {model};
  }

  return {};
}

} // namespace marianatrench

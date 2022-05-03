/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <optional>

#include <mariana-trench/Log.h>
#include <mariana-trench/shim-generator/ShimTemplates.h>

namespace marianatrench {

ShimTemplate::ShimTemplate(std::vector<ShimTarget> shim_targets)
    : shim_targets_(std::move(shim_targets)) {}

ShimTemplate ShimTemplate::from_json(
    const Json::Value& shim_json,
    Context& context) {
  std::vector<ShimTarget> shim_targets;
  for (const auto& callee :
       JsonValidation::null_or_array(shim_json, "callees")) {
    JsonValidation::validate_object(callee);

    if (auto shim_target = ShimTarget::from_json(callee, context)) {
      shim_targets.push_back(*shim_target);
    }
  }

  return ShimTemplate(std::move(shim_targets));
}

std::optional<Shim> ShimTemplate::instantiate(
    const Method* method_to_shim) const {
  LOG(5, "Instantiating ShimTemplate for {}", method_to_shim->show());

  auto shim_method = ShimMethod(method_to_shim);
  std::vector<ShimTarget> instantiated_shim_targets;

  for (const auto& target_template : shim_targets_) {
    if (auto shim_target = target_template.instantiate(shim_method)) {
      instantiated_shim_targets.push_back(*shim_target);
    } else {
      WARNING(
          1,
          "Could not initialize shim target: `{}` for method: `{}`",
          target_template,
          method_to_shim->show());
    }
  }

  if (instantiated_shim_targets.empty()) {
    return std::nullopt;
  }

  return Shim(method_to_shim, std::move(instantiated_shim_targets));
}

} // namespace marianatrench

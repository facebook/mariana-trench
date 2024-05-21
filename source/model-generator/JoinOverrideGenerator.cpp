/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Model.h>
#include <mariana-trench/model-generator/JoinOverrideGenerator.h>

namespace marianatrench {

std::vector<Model> JoinOverrideGenerator::visit_method(
    const Method* method) const {
  std::vector<Model> models;

  // Do not join models at call sites for methods with too many overrides.
  const auto& overrides = context_.overrides->get(method);
  if (overrides.size() >= options_.heuristics().join_override_threshold()) {
    models.push_back(
        Model(method, context_, Model::Mode::NoJoinVirtualOverrides));
  } else {
    const auto class_name = generator::get_class_name(method);
    if ((boost::starts_with(class_name, "Landroid") ||
         boost::starts_with(class_name, "Lcom/google") ||
         boost::starts_with(class_name, "Lkotlin/") ||
         boost::starts_with(class_name, "Ljava")) &&
        overrides.size() >=
            options_.heuristics().android_join_override_threshold()) {
      models.push_back(
          Model(method, context_, Model::Mode::NoJoinVirtualOverrides));
    }
  }

  return models;
}

} // namespace marianatrench

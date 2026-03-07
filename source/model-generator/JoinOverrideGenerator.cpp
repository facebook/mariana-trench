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
  if (overrides.size() >= context_.heuristics->join_override_threshold()) {
    models.push_back(
        Model(method, context_, Model::Mode::NoJoinVirtualOverrides));
  } else {
    const auto class_name = generator::get_class_name(method);
    if ((class_name.starts_with("Landroid") ||
         class_name.starts_with("Lcom/google") ||
         class_name.starts_with("Lkotlin/") ||
         class_name.starts_with("Ljava")) &&
        overrides.size() >=
            context_.heuristics->android_join_override_threshold()) {
      models.push_back(
          Model(method, context_, Model::Mode::NoJoinVirtualOverrides));
    }
  }

  return models;
}

} // namespace marianatrench

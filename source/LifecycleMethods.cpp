/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/LifecycleMethod.h>
#include <mariana-trench/LifecycleMethods.h>
#include <mariana-trench/Options.h>

namespace marianatrench {

void LifecycleMethods::run(Context& context) {
  std::vector<LifecycleMethod> methods;
  for (const auto& path : context.options->lifecycles_paths()) {
    auto lifecycle_definitions = JsonValidation::parse_json_file(path);
    for (const auto& lifecycle_definition :
         JsonValidation::null_or_array(lifecycle_definitions)) {
      methods.emplace_back(LifecycleMethod::from_json(lifecycle_definition));
    }
  }

  for (auto& method : methods) {
    method.create_methods(context);
  }
}

} // namespace marianatrench

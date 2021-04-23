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

void LifecycleMethods::run(
    const Options& options,
    const ClassHierarchies& class_hierarchies,
    Methods& methods) {
  std::vector<LifecycleMethod> lifecycle_methods;
  for (const auto& path : options.lifecycles_paths()) {
    auto lifecycle_definitions = JsonValidation::parse_json_file(path);
    for (const auto& lifecycle_definition :
         JsonValidation::null_or_array(lifecycle_definitions)) {
      lifecycle_methods.emplace_back(
          LifecycleMethod::from_json(lifecycle_definition));
    }
  }

  for (auto& lifecycle_method : lifecycle_methods) {
    lifecycle_method.create_methods(class_hierarchies, methods);
  }
}

} // namespace marianatrench

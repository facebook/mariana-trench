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
  std::unordered_map<std::string, LifecycleMethod> lifecycle_methods;
  for (const auto& path : options.lifecycles_paths()) {
    auto lifecycle_definitions = JsonValidation::parse_json_file(path);
    for (const auto& lifecycle_definition :
         JsonValidation::null_or_array(lifecycle_definitions)) {
      auto method = LifecycleMethod::from_json(lifecycle_definition);
      auto [_, inserted] =
          lifecycle_methods.emplace(method.method_name(), method);
      if (!inserted) {
        // Happens when another life-cycle definition has the same method name
        // as the current one. Method names must be unique across all
        // definitions because a `DexMethod` of the form
        // `ChildClass;.<method_name>` will be created per `method` defined.
        throw JsonValidationError(
            lifecycle_definition,
            /* field */ "method_name",
            "unique values across all life-cycle definitions");
      }
    }
  }

  for (auto& [_, lifecycle_method] : lifecycle_methods) {
    lifecycle_method.create_methods(class_hierarchies, methods);
  }
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/LifecycleMethods.h>
#include <mariana-trench/Options.h>

namespace marianatrench {

LifecycleMethodsError::LifecycleMethodsError(
    const Json::Value& value,
    const std::optional<std::string>& field,
    const std::string& expected)
    : JsonValidationError(value, field, expected) {}

void LifecycleMethods::run(
    const Options& options,
    const ClassHierarchies& class_hierarchies,
    Methods& methods) {
  LifecycleMethods lifecycle_methods;
  for (const auto& path : options.lifecycles_paths()) {
    lifecycle_methods.add_methods_from_json(
        JsonValidation::parse_json_file(path));
  }

  for (auto& [_, lifecycle_method] : lifecycle_methods.methods()) {
    lifecycle_method.create_methods(class_hierarchies, methods);
  }
}

void LifecycleMethods::add_methods_from_json(
    const Json::Value& lifecycle_definitions) {
  for (const auto& lifecycle_definition :
       JsonValidation::null_or_array(lifecycle_definitions)) {
    auto method = LifecycleMethod::from_json(lifecycle_definition);
    auto [_, inserted] =
        lifecycle_methods_.emplace(method.method_name(), method);
    if (!inserted) {
      // Happens when another life-cycle definition has the same method name
      // as the current one. Method names must be unique across all
      // definitions because a `DexMethod` of the form
      // `ChildClass;.<method_name>` will be created per `method` defined.
      throw LifecycleMethodsError(
          lifecycle_definition,
          /* field */ "method_name",
          "unique values across all life-cycle definitions");
    }
  }
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/CachedModelsContext.h>
#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/JsonReaderWriter.h>
#include <mariana-trench/Overrides.h>

namespace marianatrench {

CachedModelsContext::CachedModelsContext(
    Context& context,
    const Options& options)
    : is_cleared_(false) {
  auto sharded_models_directory = options.sharded_models_directory();
  if (!sharded_models_directory.has_value()) {
    // No sharded models
    return;
  }

  models_.emplace(
      Registry::from_sharded_models_json(context, *sharded_models_directory));

  // Read overrides
  auto overrides_file = options.overrides_input_path();
  if (!std::filesystem::exists(*overrides_file)) {
    throw std::runtime_error(
        "Overrides file must exist when sharded input models are provided.");
  }

  LOG(1, "Reading overrides from `{}`", overrides_file->native());
  auto overrides_json = JsonReader::parse_json_file(*overrides_file);
  overrides_ = Overrides::from_json(overrides_json, *context.methods);

  // Read class hierarchies
  auto class_hierarchies_file = options.class_hierarchies_input_path();
  if (!std::filesystem::exists(*class_hierarchies_file)) {
    throw std::runtime_error(
        "Class hierarchies file must exist when sharded input models are provided.");
  }

  LOG(1,
      "Reading class hierarchies from `{}`",
      class_hierarchies_file->native());

  auto class_hierarchies_json =
      JsonReader::parse_json_file(*class_hierarchies_file);
  class_hierarchy_ = ClassHierarchies::from_json(class_hierarchies_json);
}

void CachedModelsContext::clear() {
  overrides_.clear();
  class_hierarchy_.clear();
  models_.reset();
  is_cleared_ = true;
}

} // namespace marianatrench

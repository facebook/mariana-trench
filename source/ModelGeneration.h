/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <unordered_map>

#include <mariana-trench/Context.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/Registry.h>
#include <mariana-trench/model-generator/ModelGenerator.h>
#include <mariana-trench/model-generator/ModelGeneratorName.h>

namespace marianatrench {

class ModelGeneratorError : public std::invalid_argument {
 public:
  explicit ModelGeneratorError(const std::string& message);
};

class ModelGeneration {
 public:
  static ModelGeneratorResult run(
      Context& context,
      const Registry* MT_NULLABLE preloaded_models,
      const MethodMappings& method_mappings);

  static std::
      unordered_map<const ModelGeneratorName*, std::unique_ptr<ModelGenerator>>
      make_builtin_model_generators(
          const Registry* MT_NULLABLE preloaded_models,
          Context& context);

  static std::unordered_map<const ModelGeneratorName*, std::filesystem::path>
      get_json_model_generator_paths(Context& context);
};

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <vector>

#include <mariana-trench/Context.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/model-generator/ModelGenerator.h>

namespace marianatrench {

class ModelGeneration {
 public:
  static ModelGeneratorResult run(Context& context);

  static std::map<std::string, std::unique_ptr<ModelGenerator>>
  make_builtin_model_generators(Context& context);
};

} // namespace marianatrench

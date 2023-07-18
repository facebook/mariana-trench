/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Model.h>
#include <mariana-trench/model-generator/BuilderPatternGenerator.h>
#include <mariana-trench/model-generator/ReturnsThisAnalyzer.h>

namespace marianatrench {

std::vector<Model> BuilderPatternGenerator::visit_method(
    const Method* method) const {
  std::vector<Model> models;

  if (returns_this_analyzer::method_returns_this(method)) {
    models.push_back(
        Model(method, context_, Model::Mode::AliasMemoryLocationOnInvoke));
  }

  return models;
}

} // namespace marianatrench

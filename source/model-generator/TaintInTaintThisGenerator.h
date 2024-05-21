/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Registry.h>
#include <mariana-trench/model-generator/ModelGenerator.h>

namespace marianatrench {

class TaintInTaintThisGenerator : public MethodVisitorModelGenerator {
 public:
  explicit TaintInTaintThisGenerator(
      const std::optional<Registry>& preloaded_models,
      Context& context)
      : MethodVisitorModelGenerator("taint_in_taint_this", context),
        preloaded_models_(preloaded_models) {}

  std::vector<Model> visit_method(const Method* method) const override;

 private:
  const std::optional<Registry>& preloaded_models_;
};

} // namespace marianatrench

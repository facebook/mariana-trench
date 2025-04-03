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

class TaintInTaintOutGenerator : public MethodVisitorModelGenerator {
 public:
  explicit TaintInTaintOutGenerator(
      const Registry* MT_NULLABLE preloaded_models,
      Context& context)
      : MethodVisitorModelGenerator("taint_in_taint_out", context),
        preloaded_models_(preloaded_models) {}

  std::vector<Model> visit_method(const Method* method) const override;

 private:
  // Underlying object should be alive for the duration of
  // ModelGeneration::run()
  const Registry* MT_NULLABLE preloaded_models_;
};

} // namespace marianatrench

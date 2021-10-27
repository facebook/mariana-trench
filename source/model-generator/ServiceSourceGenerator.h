/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/model-generator/ModelGenerator.h>

namespace marianatrench {

class ServiceSourceGenerator : public ModelGenerator {
 public:
  explicit ServiceSourceGenerator(Context& context)
      : ModelGenerator("service_sources", context) {}

  std::vector<Model> emit_method_models(const Methods&) override;
};

} // namespace marianatrench

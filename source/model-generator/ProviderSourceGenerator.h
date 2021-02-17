/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/model-generator/ModelGenerator.h>

namespace marianatrench {

class ProviderSourceGenerator : public ModelGenerator {
 public:
  explicit ProviderSourceGenerator(Context& context)
      : ModelGenerator("provider_source_generator", context) {}

  std::vector<Model> run(const DexStoresVector& stores) override;
};

} // namespace marianatrench

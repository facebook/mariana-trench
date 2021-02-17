/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/model-generator/ModelGenerator.h>

namespace marianatrench {

class RandomSourceGenerator : public ModelGenerator {
 public:
  explicit RandomSourceGenerator(Context& context)
      : ModelGenerator("random_number_sources", context) {}

  std::vector<Model> run(const DexStoresVector& stores) override;
};

} // namespace marianatrench

/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Generator.h>

namespace marianatrench {

class ServiceSourceGenerator : public Generator {
 public:
  explicit ServiceSourceGenerator(Context& context)
      : Generator("service_sources", context) {}

  std::vector<Model> run(const DexStoresVector& stores) override;
};

} // namespace marianatrench

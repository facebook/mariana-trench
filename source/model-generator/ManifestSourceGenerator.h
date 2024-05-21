/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/model-generator/ModelGenerator.h>

namespace marianatrench {

class ManifestSourceGenerator : public ModelGenerator {
 public:
  explicit ManifestSourceGenerator(Context& context);

  std::vector<Model> emit_method_models(const Methods&) override;

 private:
  std::string resources_directory_;
};

} // namespace marianatrench

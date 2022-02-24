/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/model-generator/ModelGenerator.h>

namespace marianatrench {

class JoinOverrideGenerator : public MethodVisitorModelGenerator {
 public:
  explicit JoinOverrideGenerator(Context& context)
      : MethodVisitorModelGenerator("join_override_generator", context) {}

  std::vector<Model> visit_method(const Method* method) const override;
};

} // namespace marianatrench

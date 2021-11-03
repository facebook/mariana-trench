/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/model-generator/ModelGenerator.h>

namespace marianatrench {

class TaintInTaintThisGenerator : public MethodVisitorModelGenerator {
 public:
  explicit TaintInTaintThisGenerator(Context& context)
      : MethodVisitorModelGenerator("taint_in_taint_this", context) {}

  std::vector<Model> visit_method(const Method* method) const override;
};

} // namespace marianatrench

/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/model-generator/ModelGenerator.h>

namespace marianatrench {

class StructuredLoggerSinkGenerator : public MethodVisitorModelGenerator {
 public:
  explicit StructuredLoggerSinkGenerator(Context& context)
      : MethodVisitorModelGenerator("structured_logger_sinks", context) {}

  std::vector<Model> visit_method(const Method* method) const override;
};

} // namespace marianatrench

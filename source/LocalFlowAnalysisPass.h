/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/AnalysisPass.h>

namespace marianatrench {

class LocalFlowAnalysisPass final : public AnalysisPass {
 public:
  std::string_view name() const override {
    return "LocalFlowAnalysis";
  }

  void run(Context& context) override;
};

} // namespace marianatrench

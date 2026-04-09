/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>

#include <mariana-trench/AnalysisPass.h>
#include <mariana-trench/Registry.h>

namespace marianatrench {

class TaintAnalysisPass final : public AnalysisPass {
 public:
  std::string_view name() const override {
    return "TaintAnalysis";
  }

  void run(Context& context) override;

  // For test access to results.
  const Registry& registry() const {
    return *registry_;
  }

 private:
  std::unique_ptr<Registry> registry_;
};

} // namespace marianatrench

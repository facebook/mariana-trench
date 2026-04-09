/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <memory>
#include <string>
#include <string_view>

namespace marianatrench {

class Context;

enum class AnalysisPassKind {
  Taint,
};

// Parse string to enum. Throws for unknown values.
// Valid: "taint"
AnalysisPassKind analysis_pass_kind_from_string(const std::string& value);

class AnalysisPass {
 public:
  virtual ~AnalysisPass() = default;
  virtual std::string_view name() const = 0;
  virtual void run(Context& context) = 0;

  // Factory: creates a pass from enum kind.
  static std::unique_ptr<AnalysisPass> create(AnalysisPassKind kind);
};

} // namespace marianatrench

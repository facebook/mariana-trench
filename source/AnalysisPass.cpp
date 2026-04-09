/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <stdexcept>

#include <mariana-trench/AnalysisPass.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/LocalFlowAnalysisPass.h>
#include <mariana-trench/TaintAnalysisPass.h>

namespace marianatrench {

AnalysisPassKind analysis_pass_kind_from_string(const std::string& value) {
  if (value == "taint") {
    return AnalysisPassKind::Taint;
  } else if (value == "local_flow") {
    return AnalysisPassKind::LocalFlow;
  }
  throw std::invalid_argument("Unknown analysis pass: " + value);
}

std::unique_ptr<AnalysisPass> AnalysisPass::create(AnalysisPassKind kind) {
  switch (kind) {
    case AnalysisPassKind::Taint:
      return std::make_unique<TaintAnalysisPass>();
    case AnalysisPassKind::LocalFlow:
      return std::make_unique<LocalFlowAnalysisPass>();
  }
  mt_unreachable();
}

} // namespace marianatrench

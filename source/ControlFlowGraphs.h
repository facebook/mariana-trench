/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <DexClass.h>

namespace marianatrench {

class ControlFlowGraphs final {
 public:
  explicit ControlFlowGraphs(const DexStoresVector& stores);
  ControlFlowGraphs(const ControlFlowGraphs&) = delete;
  ControlFlowGraphs(ControlFlowGraphs&&) = delete;
  ControlFlowGraphs& operator=(const ControlFlowGraphs&) = delete;
  ControlFlowGraphs& operator=(ControlFlowGraphs&&) = delete;
  ~ControlFlowGraphs() = default;

  /*
   * Control flow graphs are stored by Redex and can be retrieved with
   * `Method::get_code()` and then `IRCode::cfg()`.
   */
};

} // namespace marianatrench

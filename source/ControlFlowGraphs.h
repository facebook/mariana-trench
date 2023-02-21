/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <DexClass.h>

#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

class ControlFlowGraphs final {
 public:
  explicit ControlFlowGraphs(const DexStoresVector& stores);

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ControlFlowGraphs)

  /*
   * Control flow graphs are stored by Redex and can be retrieved with
   * `Method::get_code()` and then `IRCode::cfg()`.
   */
};

} // namespace marianatrench

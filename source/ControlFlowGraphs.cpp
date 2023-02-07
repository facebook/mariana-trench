/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Walkers.h>

#include <mariana-trench/ControlFlowGraphs.h>

namespace marianatrench {

ControlFlowGraphs::ControlFlowGraphs(const DexStoresVector& stores) {
  Scope scope = build_class_scope(stores);
  scope.erase(
      std::remove_if(
          scope.begin(),
          scope.end(),
          [](DexClass* dex_class) { return dex_class->is_external(); }),
      scope.end());

  walk::parallel::code(scope, [&](DexMethod* method, IRCode& code) {
    code.build_cfg();
    code.cfg().calculate_exit_block();
  });
}

} // namespace marianatrench

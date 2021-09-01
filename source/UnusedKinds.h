/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Context.h>
#include <mariana-trench/Registry.h>

namespace marianatrench {

class UnusedKinds {
 public:
  /*
   * Before the analysis begins, a context might contain Kinds that are built
   * into the binary, or specified in a model generator, but aren't actually
   * used in any rule. These can be removed to save memory/time.
   */
  static std::unordered_set<const Kind*> remove_unused_kinds(
      Context& context,
      Registry& registry);
};

} // namespace marianatrench

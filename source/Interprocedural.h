// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <mariana-trench/Context.h>
#include <mariana-trench/Registry.h>

namespace marianatrench {

class Interprocedural final {
 public:
  static void run_analysis(Context& context, Registry& registry);
};

} // namespace marianatrench

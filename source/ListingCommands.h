/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Context.h>

namespace marianatrench {

class ListingCommands {
 public:
  // Entry point for all listing commands
  static void run(Context& context);

 private:
  // Individual listing command implementations
  static void list_all_rules(Context& context);
  static void list_all_model_generators(Context& context);
  static void list_all_kinds(Context& context);
  static void list_all_kinds_in_rules(Context& context);

  static void list_all_lifecycles(Context& context);
  
  // Helper function to get model generator paths
  static std::vector<std::string> get_model_generator_paths(Context& context);
};

} // namespace marianatrench 
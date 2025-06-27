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
  // Individual listing methods (called from MarianaTrench.cpp)
  static void list_all_rules(Context& context);
  static void list_all_model_generators(Context& context);
  static void list_all_model_generators_in_rules(Context& context);
  static void list_all_kinds(Context& context);
  static void list_all_kinds_in_rules(Context& context);
  static void list_all_filters(Context& context);
  static void list_all_features(Context& context);
  static void list_all_lifecycles(Context& context);
  
 private:
  // Helper methods
  static std::vector<std::string> parse_paths_list(const std::string& input);
};

} // namespace marianatrench 
/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/ClassHierarchies.h>
#include <mariana-trench/LifecycleMethod.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Options.h>

namespace marianatrench {

/**
 * This class adds artificial methods to simulate common framework behaviors
 * that the analysis may not otherwise be able to see or handle.
 */
class LifecycleMethods final {
 public:
  static void run(
      const Options& options,
      const ClassHierarchies& class_hierarchies,
      Methods& methods);

  LifecycleMethods() {}

  LifecycleMethods(const LifecycleMethods&) = delete;
  LifecycleMethods(LifecycleMethods&&) = delete;
  LifecycleMethods& operator=(const LifecycleMethods&) = delete;
  LifecycleMethods& operator=(LifecycleMethods&&) = delete;
  ~LifecycleMethods() = default;

  void add_methods_from_json(const Json::Value& lifecycle_definitions);

  const auto& methods() const {
    return lifecycle_methods_;
  }

 private:
  std::unordered_map<std::string, LifecycleMethod> lifecycle_methods_;
};

} // namespace marianatrench

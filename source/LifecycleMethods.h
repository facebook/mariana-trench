/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/ClassHierarchies.h>
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
};

} // namespace marianatrench

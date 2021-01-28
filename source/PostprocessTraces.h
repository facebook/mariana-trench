/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Registry.h>

namespace marianatrench {

class PostprocessTraces {
 public:
  /*
   * After an analysis, the registry might contain invalid traces because of
   * collapses in the abstract tree domain.
   *
   * # Example
   * At global iteration 1, method `f` as a source on port `Return.foo`.
   * Method `g` is analyzed and finds an issue, referring to the source from
   * `f`. The issue has the following sources:
   * ```
   * Taint{Frame(callee=`f`, callee_port=`Return.foo`, ...)}
   * ```
   *
   * At global iteration 2, the source in `f` gets collapse into port `Return`.
   * This can happen for many reasons, for instance if the source tree gets too
   * wide. Method `g` now infers an issue with the sources:
   * ```
   * Taint{
   *    Frame(callee=`f`, callee_port=`Return`, ...),
   *    Frame(callee=`f`, callee_port=`Return.foo`, ...),
   * }
   * ```
   * If we export this in our results, this would result in invalid traces
   * because in `f`, there is no more source for `Return.foo`.
   *
   * To prevent that, we remove the frame with callee port `Return.foo` here.
   */
  static void remove_collapsed_traces(
      Registry& registry,
      const Context& context);
};

} // namespace marianatrench

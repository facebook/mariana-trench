/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <functional>

#include <ConcurrentContainers.h>

#include <mariana-trench/Dependencies.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/StronglyConnectedComponents.h>

namespace marianatrench {

class Scheduler final {
 public:
  explicit Scheduler(const Methods& methods, const Dependencies& dependencies);

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Scheduler)

  /* Add methods to analyze in the work queue, in a specific order. */
  void schedule(
      const ConcurrentSet<const Method*>& methods,
      std::function<void(const Method*, std::size_t)> enqueue,
      unsigned int threads) const;

 private:
  StronglyConnectedComponents strongly_connected_components_;
};

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mutex>

#include <mariana-trench/Log.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/constraints/MethodConstraints.h>
#include <mariana-trench/model-generator/ModelGenerator.h>
#include <mariana-trench/shim-generator/ShimTemplates.h>
#include <mariana-trench/shim-generator/Shims.h>

namespace marianatrench {

class ShimGenerator final {
 public:
  ShimGenerator(
      std::unique_ptr<AllOfMethodConstraint> constraint,
      ShimTemplate shim_template,
      const Methods* methods);

  /* Returns a vector of duplicate shims. */
  void emit_method_shims(
      Shims& method_shims,
      const Methods* methods,
      const MethodMappings& method_mappings);

 private:
  std::optional<InstantiatedShim> visit_method(const Method* method) const;

  template <typename InputIt>
  void visit_methods(Shims& shims, InputIt begin, InputIt end) {
    std::mutex mutex;

    auto queue = sparta::work_queue<const Method*>([&](const Method* method) {
      auto shim = this->visit_method(method);
      if (!shim) {
        return;
      }

      LOG(5, "Adding shim: {}", *shim);
      {
        std::lock_guard<std::mutex> lock(mutex);
        shims.add_instantiated_shim(*shim);
      }
    });

    for (auto iterator = begin; iterator != end; ++iterator) {
      queue.add_item(*iterator);
    }
    queue.run_all();
  }

 private:
  std::unique_ptr<AllOfMethodConstraint> constraint_;
  ShimTemplate shim_template_;
  const Methods* methods_;
};

} // namespace marianatrench

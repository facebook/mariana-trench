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

namespace marianatrench {

class JsonShimGenerator final {
 public:
  JsonShimGenerator(
      std::unique_ptr<AllOfMethodConstraint> constraint,
      ShimTemplate shim_template,
      const Methods* methods);

  MethodToShimMap emit_method_shims(
      const Methods* methods,
      const MethodMappings& method_mappings);

 private:
  std::optional<Shim> visit_method(const Method* method) const;

  template <typename InputIt>
  MethodToShimMap visit_methods(InputIt begin, InputIt end) {
    MethodToShimMap method_to_shim;
    std::mutex mutex;

    auto queue = sparta::work_queue<const Method*>([&](const Method* method) {
      auto shim = this->visit_method(method);
      if (!shim) {
        return;
      }

      LOG(5, "Adding shim: {}", *shim);
      bool inserted;
      {
        std::lock_guard<std::mutex> lock(mutex);
        inserted = method_to_shim.emplace(shim->method(), *shim).second;
      }

      if (!inserted) {
        WARNING(
            1,
            "Shim for method: `{}` already exists. Following was not added: {}",
            method->show(),
            *shim);
      }
    });

    for (auto iterator = begin; iterator != end; ++iterator) {
      queue.add_item(*iterator);
    }
    queue.run_all();

    return method_to_shim;
  }

 private:
  std::unique_ptr<AllOfMethodConstraint> constraint_;
  ShimTemplate shim_template_;
  const Methods* methods_;
};

} // namespace marianatrench

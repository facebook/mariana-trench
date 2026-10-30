/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Registry.h>
#include <mariana-trench/model-generator/ModelGenerator.h>

namespace marianatrench {

class TaintInTaintThisGenerator : public MethodVisitorModelGenerator {
 public:
  explicit TaintInTaintThisGenerator(
      const Registry* MT_NULLABLE preloaded_models,
      Context& context)
      : MethodVisitorModelGenerator("taint_in_taint_this", context),
        preloaded_models_(preloaded_models) {}

  std::vector<Model> visit_method(const Method* method) const override;

  /**
   * Name-based part of the taint-in-taint-this rule: constructors, setter-like
   * methods (`add`, `put`, `set`, `append`, ...) and methods of `$Builder`
   * classes store their arguments into their receiver. Callers check that the
   * method is an instance method without code.
   */
  static bool stores_arguments_into_receiver(const Method* method);

 private:
  // Underlying object should be alive for the duration of
  // ModelGeneration::run()
  const Registry* MT_NULLABLE preloaded_models_;
};

} // namespace marianatrench

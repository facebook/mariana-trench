/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <boost/algorithm/string/predicate.hpp>

#include <mariana-trench/Generator.h>

namespace marianatrench {

// TODO(T70217178): this is a temp solution while number_overrides constraint is
// not available for JSON generators Should be moved to
// IPCPropagationGenerator.json

class NoJoinOverridesGenerator : public MethodVisitorGenerator {
 public:
  explicit NoJoinOverridesGenerator(Context& context)
      : MethodVisitorGenerator("no_join_overrides_propagations", context) {}

  std::vector<Model> visit_method(const Method* method) const override {
    const auto class_name = generator::get_class_name(method);
    const auto arguments = generator::get_argument_types_string(method);

    for (const auto& argument : arguments) {
      if (argument.second == "Lcom/facebook/inject/InjectorLike;") {
        return {};
      }
    }

    const auto& overrides = overrides_.get(method);
    if (overrides.size() > 20 &&
        (boost::starts_with(class_name, "Landroid/app") ||
         boost::starts_with(class_name, "Landroid/widget") ||
         boost::starts_with(class_name, "Landroid/view") ||
         boost::starts_with(class_name, "Landroid/animation") ||
         boost::starts_with(class_name, "Landroid/graphics") ||
         boost::starts_with(class_name, "Lcom/google") ||
         (boost::starts_with(class_name, "Ljava/util/") &&
          !boost::starts_with(class_name, "Ljava/util/concurrent/")) ||
         boost::starts_with(class_name, "Ljava/lang/") ||
         boost::starts_with(class_name, "Lkotlin"))) {
      auto model = Model(method, context_);
      model.add_mode(Model::Mode::NoJoinVirtualOverrides);
      return {model};
    }
    return {};
  }
};

} // namespace marianatrench

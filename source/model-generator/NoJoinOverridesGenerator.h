/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Generator.h>

namespace marianatrench {

class NoJoinOverridesGenerator : public MethodVisitorGenerator {
 public:
  explicit NoJoinOverridesGenerator(Context& context)
      : MethodVisitorGenerator("no_join_overrides_propagations", context) {}

  std::vector<Model> visit_method(const Method* method) const override;
};

} // namespace marianatrench

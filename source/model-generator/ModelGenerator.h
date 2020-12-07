/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <vector>

#include <mariana-trench/Context.h>
#include <mariana-trench/Model.h>

namespace marianatrench {

class ModelGenerator {
 public:
  static std::vector<Model> run(Context& context);
};

} // namespace marianatrench

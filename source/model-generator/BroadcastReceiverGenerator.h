/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/model-generator/ModelGenerator.h>

namespace marianatrench {

class BroadcastReceiverGenerator : public ModelGenerator {
 public:
  explicit BroadcastReceiverGenerator(Context& context)
      : ModelGenerator("broadcast_receiver_generator", context) {}

  std::vector<Model> run(const Methods&) override;
};

} // namespace marianatrench

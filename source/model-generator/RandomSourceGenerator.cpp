/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/model-generator/RandomSourceGenerator.h>

namespace {

std::unordered_set<std::string> k_random_signatures = {
    "Ljava/util/Random;.nextInt:()I",
    "Ljava/util/Random;.nextInt:(I)I",
    "Ljava/util/Random;.nextDouble:()D",
    "Ljava/util/Random;.nextFloat:()F",
    "Ljava/util/Random;.nextLong:()J",
};

} // namespace

namespace marianatrench {

std::vector<Model> RandomSourceGenerator::run(
    const DexStoresVector& /*stores*/) {
  std::vector<Model> models;

  for (const auto& random_signature : k_random_signatures) {
    // Add source model.
    const auto* method = methods_.get(random_signature);
    if (!method) {
      continue;
    }

    auto model = Model(method, context_);
    model.add_generation(
        AccessPath(Root(Root::Kind::Return)),
        generator::source(
            context_,
            method,
            /* kind */ "RandomNumber"));
    models.push_back(model);
  }

  return models;
}

} // namespace marianatrench

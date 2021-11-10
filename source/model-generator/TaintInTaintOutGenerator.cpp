/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string.hpp>
#include "mariana-trench/model-generator/ModelGenerator.h"

#include <boost/algorithm/string/predicate.hpp>
#include <mariana-trench/model-generator/ModelGenerator.h>
#include <mariana-trench/model-generator/TaintInTaintOutGenerator.h>

namespace marianatrench {

std::vector<Model> TaintInTaintOutGenerator::visit_method(
    const Method* method) const {
  if (method->get_code()) {
    return {};
  }

  const auto method_signature = show(method);
  if (boost::ends_with(method_signature, ".size:()I") ||
      boost::ends_with(method_signature, ".hashCode:()I") ||
      boost::starts_with(method_signature, "Ljava/lang/Object;.getClass:")) {
    return {};
  }

  return {Model(method, context_, Model::Mode::TaintInTaintOut)};
}

} // namespace marianatrench

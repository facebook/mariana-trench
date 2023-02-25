/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/PropagationConfig.h>

namespace marianatrench {

PropagationConfig PropagationConfig::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  JsonValidation::string(value, /* field */ "output");
  auto output = AccessPath::from_json(value["output"]);
  const PropagationKind* kind = nullptr;
  if (output.root().is_return()) {
    kind = context.kinds->local_return();
  } else if (output.root().is_argument()) {
    kind = context.kinds->local_argument(output.root().parameter_position());
  } else {
    throw JsonValidationError(
        value,
        /* field */ "output",
        "an access path with a `Return` or `Argument(x)` root");
  }

  JsonValidation::string(value, /* field */ "input");
  auto input = AccessPath::from_json(value["input"]);

  if (!input.root().is_argument()) {
    throw JsonValidationError(
        value,
        /* field */ "input",
        "an access path to an argument");
  }

  auto inferred_features = FeatureMayAlwaysSet::from_json(value, context);
  FeatureSet user_features = FeatureSet::from_json(value["features"], context);

  return PropagationConfig(
      std::move(input),
      kind,
      PathTreeDomain{{output.path(), SingletonAbstractDomain{}}},
      inferred_features,
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      user_features);
}

std::ostream& operator<<(
    std::ostream& out,
    const PropagationConfig& propagation) {
  out << "PropagationConfig(input_path=" << propagation.input_path_
      << ", kind=`" << show(propagation.kind_) << "`"
      << ", output_paths=" << propagation.output_paths_;
  if (!propagation.inferred_features_.empty()) {
    out << ", inferred_features=" << propagation.inferred_features_;
  }
  if (!propagation.locally_inferred_features_.empty()) {
    out << ", locally_inferred_features="
        << propagation.locally_inferred_features_;
  }
  if (!propagation.user_features_.empty()) {
    out << ", user_features=" << propagation.user_features_;
  }
  return out << ")";
}

} // namespace marianatrench

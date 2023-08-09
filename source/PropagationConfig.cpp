/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/PropagationConfig.h>
#include <mariana-trench/TransformsFactory.h>

namespace marianatrench {

AccessPath PropagationConfig::callee_port() const {
  if (call_info_.is_propagation_with_trace()) {
    mt_assert(call_info_.is_declaration());
    return AccessPath(Root(Root::Kind::Leaf));
  } else {
    return AccessPath(propagation_kind()->root());
  }
}

PropagationConfig PropagationConfig::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);
  JsonValidation::check_unexpected_members(
      value,
      {"output",
       "input",
       "may_features",
       "always_features",
       "features",
       "collapse",
       "collapse-depth",
       "transforms"});

  JsonValidation::string(value, /* field */ "output");
  auto output = AccessPath::from_json(value["output"]);
  const PropagationKind* propagation_kind = nullptr;
  if (output.root().is_return()) {
    propagation_kind = context.kind_factory->local_return();
  } else if (output.root().is_argument()) {
    propagation_kind = context.kind_factory->local_argument(
        output.root().parameter_position());
  } else {
    throw JsonValidationError(
        value,
        /* field */ "output",
        "an access path with a `Return` or `Argument(x)` root");
  }

  JsonValidation::string(value, /* field */ "input");
  auto input = AccessPath::from_json(value["input"]);

  if (!input.root().is_argument() &&
      !input.root().is_call_effect_for_local_propagation_input()) {
    throw JsonValidationError(
        value,
        /* field */ "input",
        "an access path to an argument or supported call effect");
  }

  auto inferred_features = FeatureMayAlwaysSet::from_json(
      value, context, /* check_unexpected_members */ false);
  FeatureSet user_features = FeatureSet::from_json(value["features"], context);

  auto collapse_depth = CollapseDepth::zero();
  if (value.isMember("collapse")) {
    collapse_depth = JsonValidation::boolean(value, "collapse")
        ? CollapseDepth::zero()
        : CollapseDepth::no_collapse();
  } else if (value.isMember("collapse-depth")) {
    int collapse_depth_integer =
        JsonValidation::integer(value, "collapse-depth");
    if (collapse_depth_integer < 0) {
      throw JsonValidationError(
          value, "collapse-depth", /* expected */ "non-negative integer");
    }
    collapse_depth = CollapseDepth(collapse_depth_integer);
  }

  const Kind* kind = nullptr;
  if (value.isMember("transforms")) {
    kind = context.kind_factory->transform_kind(
        /* base_kind */ propagation_kind,
        /* local_transforms */
        context.transforms_factory->create(
            TransformList::from_json(value["transforms"], context)),
        /* global_transforms */ nullptr);
  } else {
    kind = propagation_kind;
  }

  return PropagationConfig(
      std::move(input),
      kind,
      PathTreeDomain{{output.path(), collapse_depth}},
      inferred_features,
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      user_features);
}

std::ostream& operator<<(
    std::ostream& out,
    const PropagationConfig& propagation) {
  out << "PropagationConfig(input_path=" << propagation.input_path_
      << ", kind=`" << show(propagation.kind_) << "`"
      << ", output_paths=" << propagation.output_paths_
      << ", call_info=" << propagation.call_info_;
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

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Context.h>
#include <mariana-trench/ExtraTraceSet.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/TaintConfig.h>

namespace marianatrench {

namespace {

const AccessPath* validate_and_infer_crtex_callee_port(
    const Json::Value& value,
    const AccessPath* MT_NULLABLE callee_port,
    const CanonicalNameSetAbstractDomain& canonical_names,
    const TaggedRootSet& via_type_of_ports,
    Context& context) {
  mt_assert(canonical_names.is_value() && !canonical_names.elements().empty());

  // Anchor ports only go with templated canonical names. Producer ports only
  // go with instantiated canonical names. No other ports are allowed.
  bool is_templated = false;
  bool is_instantiated = false;
  for (const auto& canonical_name : canonical_names.elements()) {
    if (canonical_name.instantiated_value()) {
      is_instantiated = true;
    } else {
      is_templated = true;
    }
  }

  if (is_instantiated == is_templated) {
    throw JsonValidationError(
        value,
        /* field */ "canonical_names",
        "all instantiated, or all templated values, not mix of both");
  }

  if (is_templated) {
    auto num_via_type_of_ports = via_type_of_ports.size();
    for (const auto& canonical_name : canonical_names.elements()) {
      auto is_via_type_of = canonical_name.is_via_type_of_template();
      if (is_via_type_of && num_via_type_of_ports != 1) {
        throw JsonValidationError(
            value,
            /* field */ std::nullopt,
            "exactly one 'via_type_of' port when canonical name contains 'via_type_of' template");
      }
    }
  }

  // If callee_port is user-specified (not null), validate it.
  if (callee_port != nullptr) {
    if (callee_port->root().is_anchor() && is_instantiated) {
      throw JsonValidationError(
          value,
          /* field */ std::nullopt,
          "`Anchor` callee ports to go with templated canonical names.");
    } else if (callee_port->root().is_producer() && is_templated) {
      throw JsonValidationError(
          value,
          /* field */ std::nullopt,
          "`Producer` callee ports to go with instantiated canonical names.");
    } else if (!callee_port->root().is_leaf_port()) {
      throw JsonValidationError(
          value,
          /* field */ std::nullopt,
          "`Anchor` or `Producer` callee port for crtex frame with canonical_names defined.");
    }
  }

  if (callee_port == nullptr || callee_port->root().is_leaf()) {
    if (is_instantiated) {
      throw JsonValidationError(
          value,
          /* field */ std::nullopt,
          "Instantiated canonical names must have callee_port defined as `Producer.<producer_id>.<canonical_port>`");
    }

    // If the callee_port is defaulted to Leaf, it should be updated to an
    // Anchor to enable detection that this comes from a CRTEX producer.
    return context.access_path_factory->get(
        AccessPath(Root(Root::Kind::Anchor)));
  }

  // For CRTEX, the callee port is never nullptr.
  mt_assert(callee_port != nullptr);
  return callee_port;
}

} // namespace

TaintConfig TaintConfig::from_json(const Json::Value& value, Context& context) {
  JsonValidation::validate_object(value);
  JsonValidation::check_unexpected_members(
      value,
      {"port", // Only when called from `Model::from_config_json`
       "caller_port", // Only when called from `Model::from_config_json`
       "type", // Only when called from `Model::from_config_json` for effects
       "kind",
       "partial_label",
       "callee_port",
       "callee",
       "call_position",
       "distance",
       "features",
       "via_annotation",
       "via_type_of",
       "via_value_of",
       "canonical_names"});

  const Kind* kind = Kind::from_config_json(
      value, context, /* check_unexpected_members */ false);

  const AccessPath* MT_NULLABLE callee_port = nullptr;
  if (value.isMember("callee_port")) {
    auto port = JsonValidation::string(value, /* field */ "callee_port");
    if (port != "Leaf") {
      callee_port =
          context.access_path_factory->get(AccessPath::from_json(port));
    }
  }

  const Method* MT_NULLABLE callee = nullptr;
  if (value.isMember("callee")) {
    callee = Method::from_json(
        JsonValidation::object_or_string(value, /* field */ "callee"), context);
  }

  const Position* MT_NULLABLE call_position = nullptr;
  if (value.isMember("call_position")) {
    call_position = Position::from_json(
        JsonValidation::object(value, /* field */ "call_position"), context);
  }

  int distance = 0;
  if (value.isMember("distance")) {
    distance = JsonValidation::integer(value, /* field */ "distance");
  }

  // User specified always-features.
  FeatureSet user_features;
  if (value.isMember("features")) {
    JsonValidation::null_or_array(value, /* field */ "features");
    user_features = FeatureSet::from_json(value["features"], context);
  }

  // Annotation features, to be converted to user features at model instantiation time.
  AnnotationFeatureSet annotation_features;
  if (value.isMember("via_annotation")) {
    JsonValidation::null_or_array(value, /* field */ "via_annotation");
    annotation_features = AnnotationFeatureSet::from_json(value["via_annotation"], context);
  }

  TaggedRootSet via_type_of_ports;
  if (value.isMember("via_type_of")) {
    for (const auto& tagged_root :
         JsonValidation::null_or_array(value, /* field */ "via_type_of")) {
      via_type_of_ports.add(TaggedRoot::from_json(tagged_root));
    }
  }

  TaggedRootSet via_value_of_ports;
  if (value.isMember("via_value_of")) {
    for (const auto& tagged_root :
         JsonValidation::null_or_array(value, /* field */ "via_value_of")) {
      via_value_of_ports.add(TaggedRoot::from_json(tagged_root));
    }
  }

  CanonicalNameSetAbstractDomain canonical_names;
  if (value.isMember("canonical_names")) {
    for (const auto& canonical_name :
         JsonValidation::nonempty_array(value, /* field */ "canonical_names")) {
      canonical_names.add(CanonicalName::from_json(canonical_name));
    }
  }

  CallKind call_kind = CallKind::declaration();
  OriginSet origins;
  if (canonical_names.is_value() && !canonical_names.elements().empty()) {
    callee_port = validate_and_infer_crtex_callee_port(
        value, callee_port, canonical_names, via_type_of_ports, context);
    // CRTEX consumer frames (unintuitively identified by "producer" in the
    // port) are treated as origins instead of declaration so that the trace
    // to the producer issue is retained. Declaration frames would be ignored
    // by the JSON parser. The instantiated canonical name(s) and port should
    // be reported in the origins as they indicate the next hop of the trace.
    // This acts like we are propagating the call kind and canonical names.
    if (callee_port->root().is_producer()) {
      call_kind = call_kind.propagate();
      origins.join_with(
          CanonicalName::propagate(canonical_names, *callee_port));
    }
  } else if (
      callee_port != nullptr &&
      (callee_port->root().is_anchor() || callee_port->root().is_producer())) {
    throw JsonValidationError(
        value,
        /* field */ std::nullopt,
        "canonical_names to be specified with `Anchor` or `Producer` callee_port.");
  }

  // Sanity checks.
  if (callee == nullptr) {
    if (callee_port != nullptr && !callee_port->root().is_leaf_port()) {
      throw JsonValidationError(
          value,
          /* field */ "callee_port",
          /* expected */ "`Leaf`, `Anchor` or `Producer`");
    } else if (call_position != nullptr) {
      throw JsonValidationError(
          value,
          /* field */ "call_position",
          /* expected */ "unspecified position for leaf taint");
    } else if (distance != 0) {
      throw JsonValidationError(
          value,
          /* field */ "distance",
          /* expected */ "a value of 0");
    }
  } else {
    if (callee_port == nullptr || callee_port->root().is_leaf_port()) {
      throw JsonValidationError(
          value,
          /* field */ "callee_port",
          /* expected */ "`Argument(x)` or `Return`");
    } else if (call_position == nullptr) {
      throw JsonValidationError(
          value,
          /* field */ "call_position",
          /* expected */ "non-null position");
    } else if (distance == 0) {
      throw JsonValidationError(
          value,
          /* field */ "distance",
          /* expected */ "non-zero distance");
    }
  }

  return TaintConfig(
      kind,
      callee_port,
      callee,
      call_kind,
      call_position,
      // Intervals cannot be set from a json model generator.
      /* class_interval_context */ CallClassIntervalContext(),
      distance,
      // Origins are not configurable. They are auto-populated when the config
      // is applied as a model for a specific method/field.
      std::move(origins),
      /* inferred_features */ FeatureMayAlwaysSet::bottom(),
      std::move(user_features),
      std::move(annotation_features),
      std::move(via_type_of_ports),
      std::move(via_value_of_ports),
      std::move(canonical_names),
      /* output_paths */ {},
      /* local_positions */ {},
      /* locally_inferred_features */ FeatureMayAlwaysSet::bottom(),
      /* extra_traces */ {});
}

} // namespace marianatrench

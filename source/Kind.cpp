/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string/predicate.hpp>
#include <fmt/format.h>

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/LocalArgumentKind.h>
#include <mariana-trench/TriggeredPartialKind.h>

namespace marianatrench {

InvalidKindStringError::InvalidKindStringError(
    const std::string& kind,
    const std::string& expected)
    : JsonValidationError(kind, /* field */ "kind", expected) {}

KindNotSupportedError::KindNotSupportedError(
    const std::string& kind,
    const std::string& expected)
    : JsonValidationError(kind, /* field */ "kind", expected) {}

Json::Value Kind::to_json() const {
  auto value = Json::Value(Json::objectValue);
  value["kind"] = to_trace_string();
  return value;
}

std::ostream& operator<<(std::ostream& out, const Kind& kind) {
  kind.show(out);
  return out;
}

const Kind* Kind::from_json(const Json::Value& value, Context& context) {
  const auto leaf_kind =
      JsonValidation::object_or_string(value, /* field */ "kind");

  // Some kinds are represented as an object. Use unique keys in them to
  // determine the Kind.
  //
  // There is a notable asymmetry between to_json() and from_json() for Kinds
  // whose serialized form is an Object:
  // to_json() nests the value in a "kind" field to be consistent with the
  // overridden Kind::to_json().
  // from_json(value, ...) assumes `value` has been extracted from "kind" field.
  if (leaf_kind.isObject()) {
    if (leaf_kind.isMember("base")) {
      return TransformKind::from_json(leaf_kind, context);
    } else if (leaf_kind.isMember("triggered_rule")) {
      return TriggeredPartialKind::from_json(leaf_kind, context);
    } else if (leaf_kind.isMember("partial_label")) {
      // Check for "partial_label" must occur after check for "triggered_rule"
      // to differentiate between TriggeredPartialKind and PartialKind. The
      // "partial_label" key exists in both.
      return PartialKind::from_json(leaf_kind, context);
    } else {
      throw JsonValidationError(
          value,
          "kind",
          /* expected */ "TransformKind or PartialKind nested in an object.");
    }
  }

  if (value.isMember("partial_label")) {
    // "partial_label" in the outer object is a legacy format that is no longer
    // supported. It should be nested within the "kind" object.
    throw JsonValidationError(
        value,
        "partial_label",
        /* expected */ "'partial_label' nested in a 'kind' object.");
  }

  return Kind::from_trace_string(leaf_kind.asString(), context);
}

const Kind* Kind::from_config_json(
    const Json::Value& value,
    Context& context,
    bool check_unexpected_members) {
  if (check_unexpected_members) {
    JsonValidation::check_unexpected_members(value, {"kind", "partial_label"});
  }
  const auto leaf_kind = JsonValidation::string(value, /* field */ "kind");
  if (value.isMember("partial_label")) {
    return context.kind_factory->get_partial(
        leaf_kind, JsonValidation::string(value, /* field */ "partial_label"));
  } else {
    return context.kind_factory->get(leaf_kind);
  }
}

const Kind* Kind::from_trace_string(const std::string& kind, Context& context) {
  // Parsing of TriggeredPartialKind, PartialKind and TransformKind are
  // supported from the JSON, as objects rather than strings. They no longer use
  // the old string representation, so parsing them here is disabled.
  if (kind == "LocalReturn") {
    return context.kind_factory->local_return();
  } else if (boost::starts_with(kind, "LocalArgument(")) {
    return LocalArgumentKind::from_trace_string(kind, context);
  } else if (boost::starts_with(kind, "TriggeredPartial:")) {
    throw KindNotSupportedError(
        kind,
        /* expected */ "Non-TriggeredPartial Kind");
  } else if (boost::starts_with(kind, "Partial:")) {
    throw KindNotSupportedError(kind, /* expected */ "Non-Partial Kind");
  } else if (kind.find_first_of(":@") != std::string::npos) {
    throw KindNotSupportedError(
        kind,
        /* expected */ "Non-Transform Kind");
  }

  // Defaults to NamedKind.
  return context.kind_factory->get(kind);
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <mariana-trench/Sanitizer.h>
#include <mariana-trench/SourceSinkKind.h>
#include <mariana-trench/TransformOperations.h>

namespace marianatrench {

namespace {

// Returns true if the kind is a source kind and removes the prefix accordingly.
std::pair<std::string, bool> parse_source_sink_kind(
    std::string kind,
    SanitizerKind sanitizer_kind) {
  // The only case we do not already know the source/sink info is when we have a
  // propagation sanitizer
  if (sanitizer_kind == SanitizerKind::Sources) {
    mt_assert(!boost::starts_with(kind, "Source["));
    return std::make_pair(std::move(kind), true);
  } else if (sanitizer_kind == SanitizerKind::Sinks) {
    mt_assert(!boost::starts_with(kind, "Sink["));
    return std::make_pair(std::move(kind), false);
  }

  // This is a propagation sanitizer. Remove the Source[]/Sink[] wrapper from
  // kind string.
  mt_assert(sanitizer_kind == SanitizerKind::Propagations);
  mt_assert(boost::ends_with(kind, "]"));
  boost::replace_last(kind, "]", "");

  bool is_source;
  if (boost::starts_with(kind, "Source[")) {
    boost::replace_first(kind, "Source[", "");
    is_source = true;
  } else if (boost::starts_with(kind, "Sink[")) {
    boost::replace_first(kind, "Sink[", "");
    is_source = false;
  } else {
    throw JsonValidationError(
        kind,
        /* field */ std::nullopt,
        "Could not be parsed as a valid Kind for Sanitizer");
  }

  return std::make_pair(std::move(kind), is_source);
}

} // anonymous namespace

// Defined out-of-line to avoid dependency between headers
SourceSinkKind SourceSinkKind::from_transform_direction(
    const Kind* kind,
    transforms::TransformDirection direction) {
  return direction == transforms::TransformDirection::Forward ? source(kind)
                                                              : sink(kind);
}

SourceSinkKind SourceSinkKind::from_trace_string(
    const std::string& value,
    Context& context,
    SanitizerKind sanitizer_kind) {
  // Parse and remove the extra prefix from the kind string
  const auto& [value_parsed, is_source] =
      parse_source_sink_kind(value, sanitizer_kind);

  // Parse the kind as a regular kind since prefix is now gone
  const auto* kind = Kind::from_trace_string(value_parsed, context);
  return is_source ? SourceSinkKind::source(kind) : SourceSinkKind::sink(kind);
}

SourceSinkKind SourceSinkKind::from_config_json(
    const Json::Value& value,
    Context& context,
    SanitizerKind sanitizer_kind) {
  // Parse and remove the extra prefix from the kind string
  auto kind_str = JsonValidation::string(value, /* field */ "kind");
  const auto& [value_parsed, is_source] =
      parse_source_sink_kind(std::move(kind_str), sanitizer_kind);

  Json::Value value_with_parsed_kind = value;
  value_with_parsed_kind["kind"] = value_parsed;

  // Parse the kind as a regular kind since prefix is now gone
  const auto* kind = Kind::from_config_json(value_with_parsed_kind, context);
  return is_source ? SourceSinkKind::source(kind) : SourceSinkKind::sink(kind);
}

std::string SourceSinkKind::to_trace_string(
    SanitizerKind sanitizer_kind) const {
  const auto* kind = this->kind();

  // No need for prefix for non-propagation sanitizers
  if (sanitizer_kind != SanitizerKind::Propagations) {
    return kind->to_trace_string();
  }

  const auto source_or_sink = is_source() ? "Source" : "Sink";
  return fmt::format("{}[{}]", source_or_sink, kind->to_trace_string());
}

Json::Value SourceSinkKind::to_json(SanitizerKind sanitizer_kind) const {
  const auto* kind = this->kind();

  // No need for prefix for non-propagation sanitizers
  if (sanitizer_kind != SanitizerKind::Propagations) {
    return kind->to_json();
  }

  const auto source_or_sink = is_source() ? "Source" : "Sink";

  // Create the regular json for the kind, then add the source/sink prefix to
  // the kind string
  auto kind_json = kind->to_json();
  kind_json["kind"] =
      fmt::format("{}[{}]", source_or_sink, kind_json["kind"].asString());

  return kind_json;
}

std::size_t hash_value(const SourceSinkKind& kind) {
  return boost::hash<int>()(kind.hash());
}

std::ostream& operator<<(std::ostream& out, const SourceSinkKind& kind) {
  // operator<< is only used to make the PatriciaTreeSet template well-formed,
  // assume it's a propagation sanitizer for now.
  out << kind.to_trace_string(SanitizerKind::Propagations);
  return out;
}

} // namespace marianatrench

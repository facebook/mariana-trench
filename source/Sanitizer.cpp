/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Assert.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/NamedKind.h>
#include <mariana-trench/PartialKind.h>
#include <mariana-trench/Sanitizer.h>

namespace marianatrench {

Sanitizer::Sanitizer(
    const SanitizerKind& sanitizer_kind,
    const KindSetAbstractDomain& kinds)
    : sanitizer_kind_(sanitizer_kind), kinds_(kinds) {
  mt_assert(
      sanitizer_kind_ != SanitizerKind::Propagations || !kinds_.is_value());
}

bool Sanitizer::leq(const Sanitizer& other) const {
  if (is_bottom()) {
    return true;
  } else if (other.is_bottom()) {
    return false;
  }
  return sanitizer_kind_ == other.sanitizer_kind_ && kinds_.leq(other.kinds_);
}

bool Sanitizer::equals(const Sanitizer& other) const {
  if (is_bottom()) {
    return other.is_bottom();
  } else if (other.is_bottom()) {
    return false;
  } else {
    return sanitizer_kind_ == other.sanitizer_kind_ && kinds_ == other.kinds_;
  }
}

void Sanitizer::join_with(const Sanitizer& other) {
  mt_if_expensive_assert(auto previous = *this);

  if (is_bottom()) {
    *this = other;
  } else if (other.is_bottom()) {
    return;
  } else {
    mt_assert(sanitizer_kind_ == other.sanitizer_kind_);
    kinds_.join_with(other.kinds_);
  }

  mt_expensive_assert(previous.leq(*this) && other.leq(*this));
}

void Sanitizer::widen_with(const Sanitizer& other) {
  join_with(other);
}

void Sanitizer::meet_with(const Sanitizer& /* other */) {
  mt_unreachable(); // Not implemented
}

void Sanitizer::narrow_with(const Sanitizer& other) {
  meet_with(other);
}

std::ostream& operator<<(std::ostream& out, const Sanitizer& sanitizer) {
  if (sanitizer.is_bottom()) {
    return out << "Sanitizer()";
  }
  out << "Sanitizer(";
  switch (sanitizer.sanitizer_kind_) {
    case SanitizerKind::Sources:
      out << "Sources";
      break;
    case SanitizerKind::Sinks:
      out << "Sinks";
      break;
    case SanitizerKind::Propagations:
      out << "Propagations";
      break;
  }
  if (sanitizer.kinds_.is_value()) {
    out << ", kinds = " << sanitizer.kinds_;
  }
  return out << ")";
}

const Sanitizer Sanitizer::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);
  JsonValidation::check_unexpected_members(
      value,
      {"port", // Only when called from `TaintConfig::from_json`.
       "sanitize",
       "kinds"});

  SanitizerKind sanitizer_kind;
  auto sanitizer_kind_string =
      JsonValidation::string(value, /* field */ "sanitize");
  if (sanitizer_kind_string == "sources") {
    sanitizer_kind = SanitizerKind::Sources;
  } else if (sanitizer_kind_string == "sinks") {
    sanitizer_kind = SanitizerKind::Sinks;
  } else if (sanitizer_kind_string == "propagations") {
    sanitizer_kind = SanitizerKind::Propagations;
  } else {
    throw JsonValidationError(
        value,
        /* field */ "sanitizer",
        /* expected */ "`sources`, `sinks` or `propagations`");
  }

  KindSetAbstractDomain kinds;
  if (value.isMember("kinds")) {
    if (sanitizer_kind == SanitizerKind::Propagations) {
      throw JsonValidationError(
          value,
          /* field */ "kinds",
          /* expected */
          "Unspecified kinds for propagation sanitizers");
    }
    kinds = KindSetAbstractDomain();
    for (const auto& kind_json :
         JsonValidation::nonempty_array(value, "kinds")) {
      kinds.add(Kind::from_json(kind_json, context));
    }
  } else {
    kinds = KindSetAbstractDomain::top();
  }

  return Sanitizer(sanitizer_kind, kinds);
}

Json::Value Sanitizer::to_json() const {
  mt_assert(!is_bottom());
  auto value = Json::Value(Json::objectValue);
  switch (sanitizer_kind_) {
    case SanitizerKind::Sources:
      value["sanitize"] = "sources";
      break;
    case SanitizerKind::Sinks:
      value["sanitize"] = "sinks";
      break;
    case SanitizerKind::Propagations:
      value["sanitize"] = "propagations";
      break;
  }

  if (kinds_.is_value()) {
    auto kinds_json = Json::Value(Json::arrayValue);
    for (const auto* kind : kinds_.elements()) {
      kinds_json.append(kind->to_json());
    }
    value["kinds"] = kinds_json;
  }
  return value;
}

bool Sanitizer::GroupEqual::operator()(
    const Sanitizer& left,
    const Sanitizer& right) const {
  return left.sanitizer_kind() == right.sanitizer_kind();
}

std::size_t Sanitizer::GroupHash::operator()(const Sanitizer& sanitizer) const {
  return std::hash<SanitizerKind>()(sanitizer.sanitizer_kind());
}

} // namespace marianatrench

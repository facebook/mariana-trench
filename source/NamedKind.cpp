/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/KindFactory.h>
#include <mariana-trench/NamedKind.h>

namespace marianatrench {

void NamedKind::show(std::ostream& out) const {
  out << name_;
  if (subkind_.has_value()) {
    out << "(" << *subkind_ << ")";
  }
}

Json::Value NamedKind::to_json() const {
  auto value = Json::Value(Json::objectValue);
  if (subkind_.has_value()) {
    auto inner = Json::Value(Json::objectValue);
    inner["name"] = name_;
    inner["subkind"] = *subkind_;
    value["kind"] = inner;
  } else {
    value["kind"] = name_;
  }
  return value;
}

const NamedKind* NamedKind::from_inner_json(
    const Json::Value& value,
    Context& context) {
  if (value.isString()) {
    return context.kind_factory->get(value.asString());
  }
  auto name = JsonValidation::string(value, /* field */ "name");
  if (!value.isMember("subkind")) {
    throw JsonValidationError(
        value,
        "subkind",
        /* expected */
        "'subkind' when 'name' is present. "
        "Use a plain string for kinds without subkinds.");
  }
  return context.kind_factory->get(
      name, JsonValidation::string(value, /* field */ "subkind"));
}

const NamedKind* NamedKind::from_rule_json(
    const Json::Value& value,
    Context& context) {
  // Rule kinds are always plain strings (no subkind objects allowed).
  return context.kind_factory->get(JsonValidation::string(value));
}

std::string NamedKind::to_trace_string() const {
  if (subkind_.has_value()) {
    return name_ + "(" + *subkind_ + ")";
  }
  return name_;
}

const Kind* NamedKind::discard_subkind() const {
  if (!subkind_.has_value()) {
    return this;
  }
  return KindFactory::singleton().get(name_);
}

} // namespace marianatrench

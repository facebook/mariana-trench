/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string/replace.hpp>
#include <json/value.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/CanonicalName.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

std::optional<CanonicalName> CanonicalName::instantiate(
    const Method* method,
    const std::vector<const Feature*>& via_type_ofs) const {
  auto value = template_value();
  mt_assert(value);
  auto canonical_name = *value;

  if (canonical_name.find(k_leaf_name_marker) != std::string::npos) {
    auto callee_name = method->signature();
    boost::algorithm::replace_all(
        canonical_name, k_leaf_name_marker, callee_name);
  }

  if (canonical_name.find(k_via_type_of_marker) != std::string::npos) {
    if (via_type_ofs.size() == 0) {
      WARNING(
          2,
          "Could not instantiate canonical name template '{}'. Via-type-of feature not available.",
          *value);
      return std::nullopt;
    } else if (via_type_ofs.size() > 1) {
      // TODO(T90249898): Verify this does not happen during model-generation.
      // If using %via_type_of% in the template, users should only specify one
      // via_type_of port.
      ERROR(
          1,
          "Could not instantiate canonical name template '{}'. Unable to disambiguate between {} via-type-of features.",
          *value,
          via_type_ofs.size());
      return std::nullopt;
    }

    auto via_type_of = *via_type_ofs.begin();
    boost::algorithm::replace_all(
        canonical_name, k_via_type_of_marker, via_type_of->name());
  }

  return CanonicalName(CanonicalName::InstantiatedValue{canonical_name});
}

CanonicalName CanonicalName::from_json(const Json::Value& value) {
  JsonValidation::validate_object(value);

  if (value.isMember("template")) {
    auto template_value = JsonValidation::string(value["template"]);
    if (value.isMember("instantiated")) {
      throw JsonValidationError(
          value,
          /* field */ std::nullopt,
          "either 'template' or 'instantiated' value but not both.");
    }

    return CanonicalName(CanonicalName::TemplateValue{template_value});
  }

  if (value.isMember("instantiated")) {
    auto instantiated_value = JsonValidation::string(value["instantiated"]);
    return CanonicalName(CanonicalName::InstantiatedValue{instantiated_value});
  }

  throw JsonValidationError(
      value,
      /* field */ std::nullopt,
      "either 'template' or 'instantiated' value.");
}

Json::Value CanonicalName::to_json() const {
  auto result = Json::Value(Json::objectValue);
  auto value = template_value();
  if (value) {
    result["template"] = *value;
    return result;
  }

  value = instantiated_value();
  if (value) {
    result["instantiated"] = *value;
    return result;
  }

  mt_unreachable();
  return result;
}

std::ostream& operator<<(std::ostream& out, const CanonicalName& root) {
  auto value = root.template_value();
  if (value) {
    return out << "template=" << *value;
  }

  value = root.instantiated_value();
  if (value) {
    return out << "instantiated=" << *value;
  }

  mt_unreachable();
  return out;
}

} // namespace marianatrench

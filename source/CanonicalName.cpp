/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <json/value.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/CanonicalName.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

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

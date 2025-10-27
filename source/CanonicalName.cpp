/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <json/value.h>
#include <re2/re2.h>

#include <mariana-trench/AccessPathFactory.h>
#include <mariana-trench/Assert.h>
#include <mariana-trench/CanonicalName.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/OriginFactory.h>

namespace marianatrench {

namespace {

static const re2::RE2 add_underscore_regex("([a-z])([A-Z])");

void convert_to_lower_underscore(std::string& input) {
  RE2::GlobalReplace(&input, add_underscore_regex, "\\1_\\2");
  std::transform(
      input.begin(), input.end(), input.begin(), [](unsigned char c) {
        return std::tolower(c);
      });
}

} // namespace

bool CanonicalName::is_via_type_of_template() const {
  auto value = template_value();
  if (!value) {
    return false;
  }

  return value->find(k_via_type_of_marker) != std::string::npos;
}

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

  if (canonical_name.find(k_bloks_marker) != std::string::npos) {
    auto class_signature = method->get_class()->get_name()->str();
    auto pos = class_signature.find_last_of("/");
    if (pos != std::string::npos && class_signature.size() > (pos + 2)) {
      std::string class_name = str_copy(
          class_signature.substr(pos + 1, class_signature.size() - pos - 2));
      if (boost::ends_with(class_name, "Action") ||
          boost::ends_with(class_name, "Screen")) {
        class_name = class_name.substr(0, class_name.size() - 6);

        std::string method_name = str_copy(method->get_name());
        convert_to_lower_underscore(method_name);
        boost::algorithm::replace_all(
            canonical_name, k_bloks_marker, class_name + ":" + method_name);
      }
    }
  }

  // Converts Lcom/SomeMutationData;.setPhoneField:.* to
  // some_mutation:phone_field which follows the graphql notation
  if (canonical_name.find(k_graphql_root_marker) != std::string::npos) {
    auto class_signature = method->get_class()->get_name()->str();
    auto pos = class_signature.find_last_of("/");
    if (pos != std::string::npos && class_signature.size() > (pos + 2)) {
      std::string class_name = str_copy(
          class_signature.substr(pos + 1, class_signature.size() - pos - 2));
      boost::replace_last(class_name, "Data", "");
      convert_to_lower_underscore(class_name);
      std::string method_name = str_copy(method->get_name());
      boost::replace_first(method_name, "set", "");
      convert_to_lower_underscore(method_name);
      boost::algorithm::replace_all(
          canonical_name,
          k_graphql_root_marker,
          class_name + ":" + method_name);
    }
  }

  if (canonical_name.find(k_via_type_of_marker) != std::string::npos) {
    if (via_type_ofs.size() == 0) {
      WARNING(
          2,
          "Could not instantiate canonical name template '{}'. Via-type-of feature not available.",
          *value);
      return std::nullopt;
    } else if (via_type_ofs.size() > 1) {
      ERROR(
          1,
          "Could not instantiate canonical name template '{}'. Unable to disambiguate between {} via-type-of features.",
          *value,
          via_type_ofs.size());
      // Should have been verified when parsing models during model-generation.
      mt_assert(false);
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

OriginSet CanonicalName::propagate(
    const sparta::HashedSetAbstractDomain<CanonicalName>&
        instantiated_canonical_names,
    const AccessPath& callee_port) {
  OriginSet origins;
  for (const auto& name : instantiated_canonical_names.elements()) {
    origins.add(
        OriginFactory::singleton().crtex_origin(
            /* canonical_name */ *name.instantiated_value(),
            /* port */ AccessPathFactory::singleton().get(callee_port)));
  }
  return origins;
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

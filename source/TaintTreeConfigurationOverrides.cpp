/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Assert.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/TaintTreeConfigurationOverrides.h>

namespace marianatrench {

namespace {

std::string option_to_string(TaintTreeConfigurationOverrideOptions option) {
  switch (option) {
    case TaintTreeConfigurationOverrideOptions::MaxModelHeight:
      return "max_model_height";

    case TaintTreeConfigurationOverrideOptions::MaxModelWidth:
      return "max_model_width";
  }

  mt_unreachable();
}

std::optional<TaintTreeConfigurationOverrideOptions> string_to_option(
    const std::string& option) {
  if (option == "max_model_height") {
    return TaintTreeConfigurationOverrideOptions::MaxModelHeight;
  } else if (option == "max_model_width") {
    return TaintTreeConfigurationOverrideOptions::MaxModelWidth;
  } else {
    return std::nullopt;
  }
}

} // namespace

TaintTreeConfigurationOverrides::TaintTreeConfigurationOverrides(
    std::initializer_list<std::pair<
        TaintTreeConfigurationOverrideOptions,
        ScalarAbstractDomain::IntType>> options)
    : options_({}) {
  for (const auto& [option, value] : options) {
    add(option, value);
  }
}

void TaintTreeConfigurationOverrides::add(
    TaintTreeConfigurationOverrideOptions option,
    unsigned int value) {
  options_.update(option, [value](const ScalarAbstractDomain& scalar) {
    return scalar.join(ScalarAbstractDomain(value));
  });
}

TaintTreeConfigurationOverrides TaintTreeConfigurationOverrides::from_json(
    Json::Value json) {
  JsonValidation::validate_object(json);
  TaintTreeConfigurationOverrides config_overrides;

  for (const auto& option_str : json.getMemberNames()) {
    auto option = string_to_option(option_str);
    if (!option) {
      throw JsonValidationError(
          json, /* field */ option_str, "valid taint tree override option");
    }

    config_overrides.add(
        *option, JsonValidation::unsigned_integer(json, option_str));
  }

  return config_overrides;
}

Json::Value TaintTreeConfigurationOverrides::to_json() const {
  auto json = Json::Value(Json::objectValue);
  for (const auto& [option, scalar] : options_.bindings()) {
    json[option_to_string(option)] =
        Json::Value(static_cast<std::int64_t>(scalar.value()));
  }
  return json;
}

std::ostream& operator<<(
    std::ostream& out,
    const TaintTreeConfigurationOverrides& overrides) {
  if (overrides.is_bottom()) {
    return out;
  }

  out << "TaintTreeConfigurationOverrides(";

  for (const auto& [option, scalar] : overrides.options_.bindings()) {
    out << option_to_string(option) << "=" << scalar.value() << ", ";
  }

  return out << ")";
}

} // namespace marianatrench

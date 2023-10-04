/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <boost/algorithm/string.hpp>
#include <fmt/format.h>

#include <mariana-trench/JsonValidation.h>

#include <mariana-trench/model-generator/RootTemplate.h>

namespace marianatrench {


RootTemplate::RootTemplate(
    Root::Kind kind,
    std::optional<ParameterPositionTemplate> parameter_position)
    : kind_(kind), parameter_position_(parameter_position) {}

bool RootTemplate::is_argument() const {
  return kind_ == Root::Kind::Argument;
}

std::string RootTemplate::to_string() const {
  return is_argument()
      ? fmt::format("Argument({})", parameter_position_->to_string())
      : "Return";
}

Root RootTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions) const {
  switch (kind_) {
    case Root::Kind::Return: {
      return Root(Root::Kind::Return);
    }
    case Root::Kind::Argument: {
      mt_assert(parameter_position_);
      return Root(
          Root::Kind::Argument,
          parameter_position_->instantiate(parameter_positions));
    }
    default: {
      mt_unreachable();
    }
  }
}

RootTemplate RootTemplate::from_json(const Json::Value& value) {
  auto root_string = JsonValidation::string(value);
  if (boost::starts_with(root_string, "Argument(") &&
      boost::ends_with(root_string, ")") && root_string.size() >= 11) {
    auto parameter_string = root_string.substr(9, root_string.size() - 10);
    auto parameter = parse_parameter_position(parameter_string);
    if (parameter) {
      return RootTemplate{
          Root::Kind::Argument, ParameterPositionTemplate(*parameter)};
    } else {
      return RootTemplate{
          Root::Kind::Argument, ParameterPositionTemplate(parameter_string)};
    }
  } else if (root_string == "Return") {
    return RootTemplate{Root::Kind::Return};
  }

  throw JsonValidationError(
      value,
      /* field */ std::nullopt,
      fmt::format(
          "valid access path root (`Return` or `Argument(...)`), got `{}`",
          root_string));
}

}

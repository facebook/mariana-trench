/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/model-generator/ModelTemplates.h>
#include <mariana-trench/model-generator/ParameterPositionTemplate.h>

namespace marianatrench {


ParameterPositionTemplate::ParameterPositionTemplate(
    ParameterPosition parameter_position)
    : parameter_position_(parameter_position) {}

ParameterPositionTemplate::ParameterPositionTemplate(
    std::string parameter_position)
    : parameter_position_(std::move(parameter_position)) {}

std::string ParameterPositionTemplate::to_string() const {
  if (std::holds_alternative<ParameterPosition>(parameter_position_)) {
    return std::to_string(std::get<ParameterPosition>(parameter_position_));
  } else if (std::holds_alternative<std::string>(parameter_position_)) {
    return std::get<std::string>(parameter_position_);
  } else {
    mt_unreachable();
  }
}

ParameterPosition ParameterPositionTemplate::instantiate(
    const TemplateVariableMapping& parameter_positions) const {
  if (std::holds_alternative<ParameterPosition>(parameter_position_)) {
    return std::get<ParameterPosition>(parameter_position_);
  } else if (std::holds_alternative<std::string>(parameter_position_)) {
    auto result =
        parameter_positions.at(std::get<std::string>(parameter_position_));
    if (result) {
      return *result;
    } else {
      throw JsonValidationError(
          Json::Value(std::get<std::string>(parameter_position_)),
          /* field */ "parameter_position",
          /* expected */ "a variable name that is defined in \"variable\"");
    }
  } else {
    mt_unreachable();
  }
}

} // namespace marianatrench

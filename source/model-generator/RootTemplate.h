/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Access.h>
#include <mariana-trench/model-generator/ParameterPositionTemplate.h>

namespace marianatrench {

class RootTemplate final {
 public:
  explicit RootTemplate(
      Root::Kind kind,
      std::optional<ParameterPositionTemplate> parameter_position =
          std::nullopt);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(RootTemplate)

  bool is_argument() const;

  Root instantiate(
      const class TemplateVariableMapping& parameter_positions) const;

  std::string to_string() const;

  static RootTemplate from_json(const Json::Value& value);

 private:
  Root::Kind kind_;
  std::optional<ParameterPositionTemplate> parameter_position_;
};

} // namespace marianatrench

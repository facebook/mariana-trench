/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Access.h>

namespace marianatrench {

/* Store either an integer typed parameter position, or a string typed parameter
 * position (which is its name and can be instantiated when given a mapping from
 * variable names to variable indices) */
class ParameterPositionTemplate final {
 public:
  explicit ParameterPositionTemplate(ParameterPosition parameter_position);
  explicit ParameterPositionTemplate(std::string parameter_position);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ParameterPositionTemplate)

  ParameterPosition instantiate(
      const class TemplateVariableMapping& parameter_positions) const;
  std::string to_string() const;

 private:
  std::variant<ParameterPosition, std::string> parameter_position_;
};

} // namespace marianatrench

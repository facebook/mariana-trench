/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ostream>
#include <string>

#include <json/json.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Transform.h>

namespace marianatrench {

class NamedTransform final : public Transform {
 public:
  explicit NamedTransform(std::string name) : name_(std::move(name)) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(NamedTransform)

  std::string to_trace_string() const override;
  void show(std::ostream&) const override;

  static const NamedTransform* from_trace_string(
      const std::string& transform,
      Context& context);

  const auto& name() const {
    return name_;
  }

 private:
  std::string name_;
};

} // namespace marianatrench

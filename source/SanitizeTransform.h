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
#include <mariana-trench/Kind.h>
#include <mariana-trench/Transform.h>

namespace marianatrench {

class SanitizeTransform final : public Transform {
 public:
  explicit SanitizeTransform(const Kind* kind) : sanitizer_kind_(kind) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(SanitizeTransform)

  std::string to_trace_string() const override;
  void show(std::ostream&) const override;

  static const SanitizeTransform* from_trace_string(
      const std::string& transform,
      Context& context);

  static const SanitizeTransform* from_config_json(
      const Json::Value& transform,
      Context& context);

  const Kind* kind() const {
    return sanitizer_kind_;
  }

 private:
  const Kind* sanitizer_kind_;
};

class SanitizeTransformCompare {
 public:
  bool operator()(const SanitizeTransform* lhs, const SanitizeTransform* rhs)
      const {
    return lhs->kind()->to_trace_string() < rhs->kind()->to_trace_string();
  }
};

} // namespace marianatrench

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

class SourceAsTransform final : public Transform {
 public:
  explicit SourceAsTransform(const Kind* kind) : source_kind_(kind) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(SourceAsTransform)

  std::string to_trace_string() const override;
  void show(std::ostream&) const override;

  static const SourceAsTransform* from_trace_string(
      const std::string& transform,
      Context& context);

 private:
  const Kind* source_kind_;
};

} // namespace marianatrench

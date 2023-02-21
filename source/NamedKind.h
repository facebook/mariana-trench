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

namespace marianatrench {

/**
 * A simple source or sink identified only by its name. Most sources/sinks
 * would fall under this category.
 */
class NamedKind final : public Kind {
 public:
  explicit NamedKind(std::string name) : name_(std::move(name)) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(NamedKind)

  void show(std::ostream&) const override;

  static const NamedKind* from_json(const Json::Value& value, Context& context);
  std::string to_trace_string() const override;

  const auto& name() const {
    return name_;
  }

 private:
  std::string name_;
};

} // namespace marianatrench

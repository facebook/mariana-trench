/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ostream>
#include <string>

#include <json/json.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/Kind.h>

namespace marianatrench {

/**
 * A simple source or sink identified only by its name. Most sources/sinks
 * would fall under this category.
 */
class NamedKind : public Kind {
 public:
  explicit NamedKind(std::string name) : name_(std::move(name)) {}
  NamedKind(const NamedKind&) = delete;
  NamedKind(NamedKind&&) = delete;
  NamedKind& operator=(const NamedKind&) = delete;
  NamedKind& operator=(NamedKind&&) = delete;
  ~NamedKind() override = default;

  void show(std::ostream&) const override;

  static const NamedKind* from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const override;

  const auto& name() const {
    return name_;
  }

 private:
  std::string name_;
};

} // namespace marianatrench

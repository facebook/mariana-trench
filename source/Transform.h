/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>

#include <json/json.h>

#include <mariana-trench/Compiler.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>

namespace marianatrench {

/**
 * Base class that represents a single Transform operation applied on a taint
 * kind.
 */
class Transform {
 public:
  Transform() = default;
  Transform(const Transform&) = delete;
  Transform(Transform&&) = delete;
  Transform& operator=(const Transform&) = delete;
  Transform& operator=(Transform&&) = delete;
  virtual ~Transform() = default;

  virtual std::string to_trace_string() const = 0;
  virtual void show(std::ostream&) const = 0;

  template <typename T>
  const T* MT_NULLABLE as() const {
    static_assert(std::is_base_of<Transform, T>::value, "invalid as<T>");
    return dynamic_cast<const T*>(this);
  }

  template <typename T>
  bool is() const {
    static_assert(std::is_base_of<Transform, T>::value, "invalid is<T>");
    return dynamic_cast<const T*>(this) != nullptr;
  }

  static const Transform* from_json(const Json::Value& value, Context& context);
  static const Transform* from_trace_string(
      const std::string& kind,
      Context& context);

  friend std::ostream& operator<<(
      std::ostream& out,
      const Transform& transform);
};

} // namespace marianatrench

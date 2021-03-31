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

#include <mariana-trench/Compiler.h>

namespace marianatrench {

/**
 * The kind of a source or a sink (e.g, UserControlledInput)
 */
class Kind {
 public:
  Kind() = default;
  Kind(const Kind&) = delete;
  Kind(Kind&&) = delete;
  Kind& operator=(const Kind&) = delete;
  Kind& operator=(Kind&&) = delete;
  virtual ~Kind() = default;

  virtual void show(std::ostream&) const = 0;

  template <typename T>
  const T* MT_NULLABLE as() const {
    return dynamic_cast<const T*>(this);
  }

  template <typename T>
  T* MT_NULLABLE as() {
    return dynamic_cast<T*>(this);
  }

  /* For the equivalent from_json, use the appropriate method for the specific
   * kind. */
  virtual Json::Value to_json() const;

  /**
   * String value used for connecting traces of the same kind.
   * Each instance of each kind should have a unique string representation.
   */
  virtual std::string to_trace_string() const = 0;

 private:
  friend std::ostream& operator<<(std::ostream& out, const Kind& kind);
};

} // namespace marianatrench

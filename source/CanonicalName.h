/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>

#include <boost/array.hpp>
#include <boost/container_hash/extensions.hpp>
#include <json/json.h>

namespace marianatrench {

/**
 * Represents a canonical name format for a crtex leaf frame. The format is
 * determined using placeholders and the actual name will be materialized when
 * the leaf is propagated.
 * TODO: Implement and document supported placeholders.
 */
class CanonicalName final {
 public:
  explicit CanonicalName(std::string value) : value_(std::move(value)) {}

  CanonicalName(const CanonicalName&) = default;
  CanonicalName(CanonicalName&&) = default;
  CanonicalName& operator=(const CanonicalName&) = default;
  CanonicalName& operator=(CanonicalName&&) = default;
  ~CanonicalName() = default;

  bool operator==(const CanonicalName& other) const {
    return value_ == other.value_;
  }

  bool operator!=(const CanonicalName& other) const {
    return value_ != other.value_;
  }

  const std::string& value() const {
    return value_;
  }

  static CanonicalName from_json(const Json::Value& value);

 private:
  friend std::ostream& operator<<(std::ostream& out, const CanonicalName& root);

 private:
  std::string value_;
};

} // namespace marianatrench

template <>
struct std::hash<marianatrench::CanonicalName> {
  std::size_t operator()(const marianatrench::CanonicalName& name) const {
    return std::hash<std::string>()(name.value());
  }
};

/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <variant>

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
  struct TemplateValue {
    std::string value;

    bool operator==(const TemplateValue& other) const {
      return value == other.value;
    }

    bool operator!=(const TemplateValue& other) const {
      return value != other.value;
    }
  };
  struct InstantiatedValue {
    std::string value;

    bool operator==(const InstantiatedValue& other) const {
      return value == other.value;
    }

    bool operator!=(const InstantiatedValue& other) const {
      return value != other.value;
    }
  };

 public:
  explicit CanonicalName(const TemplateValue& template_value)
      : value_(template_value) {}

  explicit CanonicalName(const InstantiatedValue& instantiated_value)
      : value_(instantiated_value) {}

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

  std::optional<std::string> template_value() const {
    if (std::holds_alternative<TemplateValue>(value_)) {
      return std::get<TemplateValue>(value_).value;
    } else {
      return std::nullopt;
    }
  }

  std::optional<std::string> instantiated_value() const {
    if (std::holds_alternative<InstantiatedValue>(value_)) {
      return std::get<InstantiatedValue>(value_).value;
    } else {
      return std::nullopt;
    }
  }

  static CanonicalName from_json(const Json::Value& value);
  Json::Value to_json() const;

 private:
  friend std::ostream& operator<<(std::ostream& out, const CanonicalName& root);

 private:
  std::variant<TemplateValue, InstantiatedValue> value_;
};

} // namespace marianatrench

template <>
struct std::hash<marianatrench::CanonicalName> {
  std::size_t operator()(const marianatrench::CanonicalName& name) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, name.template_value());
    boost::hash_combine(seed, name.instantiated_value());
    return seed;
  }
};
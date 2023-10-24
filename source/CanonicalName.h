/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
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

#include <sparta/HashedSetAbstractDomain.h>

#include <mariana-trench/Feature.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/OriginSet.h>

namespace marianatrench {

/**
 * Represents a canonical name format for a crtex leaf frame. The format is
 * determined using placeholders and the actual name will be materialized when
 * the leaf is propagated.
 *
 * The templated form supports markers which will be replaced by the actual
 * value in the call to `instantiate`.
 */
class CanonicalName final {
 private:
  // Leaf name marker. Will be replaced by the full signature of `method`
  // passed to `instantiate`.
  static constexpr std::string_view k_leaf_name_marker =
      "%programmatic_leaf_name%";

  static constexpr std::string_view k_graphql_root_marker = "%graphql_root%";

  // Via-type-of marker. Will be replaced by the feature(s) passed to
  // `instantiate`. Currently supports only one feature.
  static constexpr std::string_view k_via_type_of_marker = "%via_type_of%";

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

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(CanonicalName)

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

  bool is_via_type_of_template() const;

  /**
   * Determines the full form of this canonical name. Creates a `CanonicalName`
   * with the instantiated value.
   *
   * `method` should be the Method that has the templated canonical_name defined
   * in its model.
   *
   * `via_type_ofs` is the features to be used for the "%via_type_of%"
   * placeholder. Consider deprecating to enable instantiation during
   * model-generation rather than in `Frame::propagate`.
   *
   * Returns `std::nullopt` if unable to instantiate.
   */
  std::optional<CanonicalName> instantiate(
      const Method* method,
      const std::vector<const Feature*>& via_type_ofs) const;

  static CanonicalName from_json(const Json::Value& value);
  Json::Value to_json() const;

  /**
   * A set of instantiated canonical names will be propagated as crtex
   * origins.
   */
  static OriginSet propagate(
      const sparta::HashedSetAbstractDomain<CanonicalName>&
          instantiated_canonical_names,
      const AccessPath& callee_port);

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

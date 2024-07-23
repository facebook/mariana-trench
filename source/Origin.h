/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <DexClass.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Field.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Method.h>

namespace marianatrench {

/**
 * Represents the origin of a taint, e.g. a method, field, argument, etc.
 * declared as tainted by the user.
 */
class Origin {
 public:
  Origin() = default;
  Origin(const Origin&) = delete;
  Origin(Origin&&) = delete;
  Origin& operator=(const Origin&) = delete;
  Origin& operator=(Origin&&) = delete;
  virtual ~Origin() = default;

  template <typename T>
  const T* MT_NULLABLE as() const {
    static_assert(std::is_base_of<Origin, T>::value, "invalid as<T>");
    return dynamic_cast<const T*>(this);
  }

  template <typename T>
  bool is() const {
    static_assert(std::is_base_of<Origin, T>::value, "invalid is<T>");
    return dynamic_cast<const T*>(this) != nullptr;
  }

  virtual std::string to_string() const = 0;
  virtual std::string to_model_validator_string() const = 0;
  virtual Json::Value to_json() const = 0;
  static const Origin* from_json(const Json::Value& value, Context& context);
};

class MethodOrigin final : public Origin {
 public:
  explicit MethodOrigin(const Method* method, const AccessPath* port)
      : method_(method), port_(port) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(MethodOrigin)

  std::string to_string() const override;

  std::string to_model_validator_string() const override;

  Json::Value to_json() const override;

  const Method* method() const {
    return method_;
  }

  const AccessPath* port() const {
    return port_;
  }

 private:
  const Method* method_;
  const AccessPath* port_;
};

class FieldOrigin final : public Origin {
 public:
  explicit FieldOrigin(const Field* field) : field_(field) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(FieldOrigin)

  std::string to_string() const override;

  std::string to_model_validator_string() const override;

  Json::Value to_json() const override;

  const Field* field() const {
    return field_;
  }

 private:
  const Field* field_;
};

/**
 * Represents an origin for Cross-Repo Taint EXchange (CRTEX).
 * CRTEX is a scenario intended to work with other static analysis tools, in
 * which taint flow is detected across repositories. An origin frame that
 * contains a CRTEX origin is one which connects to the traces of a different
 * run. The connection point is represented by a canonical name/port as stored
 * in this class.
 */
class CrtexOrigin final : public Origin {
 public:
  explicit CrtexOrigin(const DexString* canonical_name, const AccessPath* port)
      : canonical_name_(canonical_name), port_(port) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(CrtexOrigin)

  std::string to_string() const override;

  std::string to_model_validator_string() const override;

  Json::Value to_json() const override;

 private:
  const DexString* canonical_name_;
  const AccessPath* port_;
};

/**
 * Represents a generic origin that refers to a user-declared callee.
 */
class StringOrigin final : public Origin {
 public:
  explicit StringOrigin(const DexString* name) : name_(name) {}

  std::string to_string() const override;

  std::string to_model_validator_string() const override;

  Json::Value to_json() const override;

 private:
  const DexString* name_;
};

/**
 * Represents the origin of the source-as-transform sinks for the exploitability
 * rules. See PartiallyFulfilledExploitabilityRuleState.h for how
 * source-as-transform sinks are materialized.
 *
 * Similar to how method/field/string origins are first added when creating
 * user-declared models (see Taint::add_origins_if_declaration()),
 * exploitability-origins are added when we first infer the source-as-transform
 * sinks. It tracks the (caller method + sink callee) pair where
 * the source-as-transform sink was materialized/originated.
 *
 * From the SAPP UI perspective: Similar to how method/field/string origins
 * indicates the leaf frames of the source/sink traces, exploitability-origins
 * indicates the leaf of the call-chain portion of the exploitability trace.
 *
 * But since the SAPP UI only has the notion of source/sink traces for an issue,
 * the traces for the exploitability rules are "flatten" to a single sink trace.
 * The transition from the call-chain trace to the taint-flow sink trace is
 * modelled using a taint transform. So, exploitability-origin is ignored by
 * SAPP.
 */
class ExploitabilityOrigin final : public Origin {
 public:
  explicit ExploitabilityOrigin(
      const Method* exploitability_root,
      const DexString* callee)
      : exploitability_root_(exploitability_root), callee_(callee) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ExploitabilityOrigin)

  std::string to_string() const override;

  std::string to_model_validator_string() const override;

  Json::Value to_json() const override;

  const DexString* callee() const {
    return callee_;
  }

  const Method* exploitability_root() const {
    return exploitability_root_;
  }

  std::string issue_handle_callee() const;

 private:
  const Method* exploitability_root_;
  const DexString* callee_;
};

} // namespace marianatrench

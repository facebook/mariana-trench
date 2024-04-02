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
  virtual Json::Value to_json() const = 0;
  static const Origin* from_json(const Json::Value& value, Context& context);
};

class MethodOrigin final : public Origin {
 public:
  explicit MethodOrigin(const Method* method, const AccessPath* port)
      : method_(method), port_(port) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(MethodOrigin)

  std::string to_string() const override;

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

  Json::Value to_json() const override;

 private:
  const DexString* name_;
};

} // namespace marianatrench

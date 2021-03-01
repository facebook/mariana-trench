/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <boost/container/flat_map.hpp>
#include <json/json.h>

#include <DexClass.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/Context.h>

namespace marianatrench {

/**
 * Mapping from parameter to type that we want to override.
 *
 * The parameter position does not count `this` as a parameter.
 */
using ParameterTypeOverrides =
    boost::container::flat_map<ParameterPosition, const DexType*>;

/**
 * Represents a dex method with parameter type overrides.
 */
class Method final {
 public:
  explicit Method(
      const DexMethod* method,
      ParameterTypeOverrides parameter_type_overrides);

  Method(const Method&) = default;
  Method(Method&&) = default;
  Method& operator=(const Method&) = default;
  Method& operator=(Method&&) = default;
  ~Method() = default;

  bool operator==(const Method& other) const;

  const DexMethod* dex_method() const {
    return method_;
  }

  const ParameterTypeOverrides& parameter_type_overrides() const {
    return parameter_type_overrides_;
  }

  const IRCode* MT_NULLABLE get_code() const;
  DexType* get_class() const;
  DexProto* get_proto() const;
  const std::string& get_name() const;

  DexAccessFlags get_access() const;
  bool is_public() const;
  bool is_static() const;
  bool is_native() const;
  bool is_interface() const;

  bool is_constructor() const;
  bool returns_void() const;

  const std::string& signature() const;
  const std::string& show() const;

  /**
   * Return the number of parameters, including the implicit `this` parameter.
   */
  ParameterPosition number_of_parameters() const;

  /**
   * Return the type of the given argument.
   *
   * Note that the implicit `this` argument has the index 0.
   *
   * Returns `nullptr` if the index is invalid.
   */
  DexType* MT_NULLABLE parameter_type(ParameterPosition argument) const;

  /* Return the index of the first parameter, excluding the implicit `this`
   * parameter. */
  ParameterPosition first_parameter_index() const;

  static const Method* from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

 private:
  friend struct std::hash<Method>;
  friend std::ostream& operator<<(std::ostream& out, const Method& method);

  const DexMethod* method_;
  ParameterTypeOverrides parameter_type_overrides_;
  std::string signature_;
  std::string show_cached_;
};

} // namespace marianatrench

template <>
struct std::hash<marianatrench::Method> {
  std::size_t operator()(const marianatrench::Method& method) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, method.method_);
    boost::hash_combine(seed, method.parameter_type_overrides_.size());
    for (const auto& entry : method.parameter_type_overrides_) {
      boost::hash_combine(seed, entry.first);
      boost::hash_combine(seed, entry.second);
    }
    return seed;
  }
};

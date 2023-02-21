/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <DexClass.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

/**
 * Represents a DexField
 */
class Field final {
 public:
  explicit Field(const DexField* field);

  INCLUDE_DEFAULT_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Field)

  bool operator==(const Field& other) const;

  const DexField* dex_field() const {
    return field_;
  }

  DexType* get_class() const;
  std::string_view get_name() const;
  const std::string& show() const;

  static const Field* from_json(const Json::Value& value, Context& context);
  Json::Value to_json() const;

 private:
  friend struct std::hash<Field>;
  friend std::ostream& operator<<(std::ostream& out, const Field& field);

  const DexField* field_;
  // Of the form <class_name>;.<field_name>:<field_type>;
  std::string show_cached_;
};

} // namespace marianatrench

template <>
struct std::hash<marianatrench::Field> {
  std::size_t operator()(const marianatrench::Field& field) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, field.field_);
    return seed;
  }
};

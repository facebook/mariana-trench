/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/FieldSet.h>
#include <mariana-trench/Fields.h>
#include <mariana-trench/JsonValidation.h>

namespace marianatrench {

FieldSet::FieldSet(const Fields& fields) : set_(fields.begin(), fields.end()) {}

FieldSet FieldSet::from_json(const Json::Value& value, Context& context) {
  FieldSet fields;
  for (const auto& field_value : JsonValidation::null_or_array(value)) {
    fields.add(Field::from_json(field_value, context));
  }
  return fields;
}

Json::Value FieldSet::to_json() const {
  auto fields = Json::Value(Json::arrayValue);
  for (const auto* field : set_) {
    fields.append(field->to_json());
  }
  return fields;
}

std::ostream& operator<<(std::ostream& out, const FieldSet& fields) {
  if (fields.is_top()) {
    return out << "T";
  }
  out << "{";
  for (auto iterator = fields.begin(), end = fields.end(); iterator != end;) {
    out << "`" << show(*iterator) << "`";
    ++iterator;
    if (iterator != end) {
      out << ", ";
    }
  }
  return out << "}";
}

} // namespace marianatrench

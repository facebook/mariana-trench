/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Show.h>

#include <mariana-trench/Field.h>
#include <mariana-trench/Fields.h>
#include <mariana-trench/Redex.h>

namespace marianatrench {

Field::Field(const DexField* field)
    : field_(field), show_cached_(::show(field)){};

bool Field::operator==(const Field& other) const {
  return field_ == other.field_;
}

DexType* Field::get_class() const {
  return field_->get_class();
}

const std::string& Field::get_name() const {
  return field_->get_name()->str();
}

const std::string& Field::show() const {
  return show_cached_;
}

const Field* Field::from_json(const Json::Value& value, Context& context) {
  if (!value.isString()) {
    throw JsonValidationError(
        value, /* field */ std::nullopt, /* expected */ "string");
  }
  const auto* dex_field = redex::get_field(value.asString());
  if (!dex_field) {
    throw JsonValidationError(
        value,
        /* field */ std::nullopt,
        /* expected */ "existing field name");
  }
  return context.fields->get(dex_field->as_def());
}

Json::Value Field::to_json() const {
  return Json::Value(show_cached_);
}

std::ostream& operator<<(std::ostream& out, const Field& field) {
  return out << field.show_cached_;
}

} // namespace marianatrench

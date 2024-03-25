/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/model-generator/FieldModelTemplate.h>

namespace marianatrench {

FieldModelTemplate::FieldModelTemplate(const FieldModel& field_model)
    : field_model_(field_model) {
  mt_assert(field_model.field() == nullptr);
}

void FieldModelTemplate::add_model_generator(
    const ModelGeneratorName* model_generator) {
  field_model_.add_model_generator(model_generator);
}

std::optional<FieldModel> FieldModelTemplate::instantiate(
    const Field* field) const {
  auto field_model = field_model_.instantiate(field);
  return !field_model.empty() ? std::optional<FieldModel>{field_model}
                              : std::nullopt;
}

FieldModelTemplate FieldModelTemplate::from_json(
    const Json::Value& field_model,
    Context& context) {
  JsonValidation::validate_object(field_model);
  return FieldModelTemplate(FieldModel::from_config_json(
      /* field */ nullptr, field_model, context));
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Context.h>
#include <mariana-trench/FieldModel.h>

namespace marianatrench {

class FieldModelTemplate final {
 public:
  /* The given `field_model` must not be associated with a field. */
  explicit FieldModelTemplate(const FieldModel& field_model);
  FieldModelTemplate(const FieldModelTemplate& other) = delete;
  FieldModelTemplate(FieldModelTemplate&& other) = default;
  FieldModelTemplate& operator=(const FieldModelTemplate& other) = delete;
  FieldModelTemplate& operator=(FieldModelTemplate&& other) = delete;

  /* Create a FieldModel with information that is associated with a field. */
  std::optional<FieldModel> instantiate(const Field* field) const;
  static FieldModelTemplate from_json(
      const Json::Value& model_generator,
      Context& context);

 private:
  FieldModel field_model_;
};

} // namespace marianatrench

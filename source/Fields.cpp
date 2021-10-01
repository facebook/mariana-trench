/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Walkers.h>

#include <mariana-trench/Fields.h>

namespace marianatrench {

Fields::Fields(const DexStoresVector& stores) {
  for (const auto& scope : DexStoreClassesIterator(stores)) {
    walk::parallel::classes(scope, [&](const DexClass* klass) {
      FieldNameToTypeMap fields;
      for (const auto* field : klass->get_all_fields()) {
        fields.emplace(field->get_name(), field->get_type());
      }

      if (!fields.empty()) {
        fields_.emplace(
            klass->get_type(),
            std::make_unique<FieldNameToTypeMap>(std::move(fields)));
      }
    });
  }
}

const DexType* MT_NULLABLE
Fields::field_type(const DexType* klass, const DexString* field) const {
  const auto* field_name_to_type = fields_.get(klass, /* default */ nullptr);

  if (field_name_to_type != nullptr) {
    auto result = field_name_to_type->find(field);
    if (result != field_name_to_type->end()) {
      return result->second;
    }
  }

  return nullptr;
}

} // namespace marianatrench

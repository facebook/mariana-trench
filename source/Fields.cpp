/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <Walkers.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Fields.h>

namespace {

/**
 * Returns all known DexTypes in the hierarchy of `type` (ancestors and
 * descendents).
 */
std::unordered_set<const DexType*> types_in_class_hierarchy(
    const DexType* type,
    const marianatrench::ClassHierarchies& class_hierarchies) {
  mt_assert(type != type::java_lang_Object());

  const auto* klass = type_class(type);
  if (klass == nullptr) {
    // Not an object type, or class does not exist in the APK (might be in
    // the system jars). No class hierarchy.
    return std::unordered_set<const DexType*>{};
  }

  // Include self + descendants.
  auto types = class_hierarchies.extends(type);
  types.insert(type);

  // Include inherited types.
  const auto* super_class_type = klass->get_super_class();
  while (super_class_type) {
    klass = type_class(super_class_type);
    if (klass == nullptr) {
      // This can happen if the class does not exist in the APK (e.g. exists in
      // jar). Stop here. We do not have enough information about `klass`.
      break;
    }
    types.insert(super_class_type);
    super_class_type = klass->get_super_class();
  }

  return types;
}

} // namespace

namespace marianatrench {

Fields::Fields(
    const ClassHierarchies& class_hierarchies,
    const DexStoresVector& stores) {
  using FieldNameToTypeMap =
      std::unordered_map<const DexString*, const DexType*>;

  // Compute map class_type -> field -> type.
  // Class hierarchy is ignored at this stage.
  ConcurrentMap<const DexType*, FieldNameToTypeMap> fields_in_class;
  for (const auto& scope : DexStoreClassesIterator(stores)) {
    walk::parallel::classes(scope, [&](const DexClass* klass) {
      FieldNameToTypeMap field_to_type;
      for (const auto* field : klass->get_all_fields()) {
        field_to_type.emplace(field->get_name(), field->get_type());
      }

      if (!field_to_type.empty()) {
        fields_in_class.emplace(klass->get_type(), std::move(field_to_type));
      }
    });
  }

  // Use class hierarchy to compute
  // class_type -> derived/inherited fields -> type
  for (const auto& scope : DexStoreClassesIterator(stores)) {
    walk::parallel::classes(scope, [&](const DexClass* klass) {
      const auto* type = klass->get_type();
      if (type == type::java_lang_Object()) {
        return;
      }
      auto class_types = types_in_class_hierarchy(type, class_hierarchies);
      FieldTypeMap all_field_types;
      for (const auto* class_type : class_types) {
        auto field_types = fields_in_class.find(class_type);
        if (field_types == fields_in_class.end()) {
          // No fields in this class.
          continue;
        }
        for (const auto& [field, field_type] : field_types->second) {
          all_field_types[field].insert(field_type);
        }
      }

      fields_.emplace(
          klass->get_type(),
          std::make_unique<FieldTypeMap>(std::move(all_field_types)));
    });
  }
}

const Fields::Types& Fields::field_types(
    const DexType* klass,
    const DexString* field) const {
  const auto* field_name_to_type = fields_.get(klass, /* default */ nullptr);

  if (field_name_to_type != nullptr) {
    auto result = field_name_to_type->find(field);
    if (result != field_name_to_type->end()) {
      return result->second;
    }
  }

  return empty_types_;
}

} // namespace marianatrench

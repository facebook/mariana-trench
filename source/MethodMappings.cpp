/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <sparta/SpartaWorkQueue.h>

#include <mariana-trench/MethodMappings.h>
#include <mariana-trench/Redex.h>
#include <mariana-trench/model-generator/ModelGenerator.h>

namespace marianatrench {
namespace {

void create_name_to_method(
    const Method* method,
    ConcurrentMap<std::string_view, MethodHashedSet>& method_mapping) {
  auto method_name = method->get_name();
  method_mapping.update(
      method_name,
      [&](std::string_view /* name */,
          MethodHashedSet& methods,
          bool /* exists */) { methods.add(method); });
}

void create_class_to_method(
    const Method* method,
    ConcurrentMap<std::string_view, MethodHashedSet>& method_mapping) {
  auto parent_class = method->get_class()->get_name()->str();
  method_mapping.update(
      parent_class,
      [&](std::string_view /* parent_name */,
          MethodHashedSet& methods,
          bool /* exists */) { methods.add(method); });
}

void create_class_to_override_method(
    const Method* method,
    ConcurrentMap<std::string_view, MethodHashedSet>& method_mapping) {
  auto class_name = method->get_class()->get_name()->str();
  auto* dex_class = redex::get_class(class_name);
  if (!dex_class) {
    return;
  }
  std::unordered_set<std::string_view> parent_classes =
      generator::get_parents_from_class(
          dex_class, /* include_interfaces */ true);
  parent_classes.insert(class_name);
  for (const auto& parent_class : parent_classes) {
    method_mapping.update(
        parent_class,
        [&](std::string_view /* parent_name */,
            MethodHashedSet& methods,
            bool /* exists */) { methods.add(method); });
  }
}

void create_signature_to_method(
    const Method* method,
    ConcurrentMap<std::string, MethodHashedSet>& method_mapping) {
  std::string signature = method->signature();
  method_mapping.update(
      signature,
      [&](const std::string& /* parent_name */,
          MethodHashedSet& methods,
          bool /* exists */) { methods.add(method); });
}

void create_annotation_type_to_method(
    const Method* method,
    ConcurrentMap<std::string_view, MethodHashedSet>& method_mapping) {
  std::string signature = method->signature();
  const DexAnnotationSet* annotations_set =
      method->dex_method()->get_anno_set();
  if (!annotations_set) {
    return;
  }
  for (const auto& annotation : annotations_set->get_annotations()) {
    DexType* annotation_type = annotation->type();
    if (!annotation_type) {
      continue;
    }
    method_mapping.update(
        annotation_type->str(),
        [&](std::string_view /* annotation_type */,
            MethodHashedSet& methods,
            bool /* exists */) { methods.add(method); });
  }
}

} // namespace

void MethodMappings::create_mappings_for_method(const Method* method) {
  create_name_to_method(method, name_to_methods_);
  create_class_to_method(method, class_to_methods_);
  create_class_to_override_method(method, class_to_override_methods_);
  create_signature_to_method(method, signature_to_methods_);
  create_annotation_type_to_method(method, annotation_type_to_methods_);
}

MethodMappings::MethodMappings(const Methods& methods) {
  auto queue = sparta::work_queue<const Method*>(
      [&](const Method* method) { create_mappings_for_method(method); });
  for (const auto* method : methods) {
    all_methods_.add(method);
    queue.add_item(method);
  }
  queue.run_all();
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/Constants.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/model-generator/DFASourceGenerator.h>

namespace marianatrench {

namespace {
bool is_class_accessible_via_dfa(const DexClass* clazz) {
  if (!clazz->get_anno_set()) {
    return false;
  }

  auto dfa_annotation_type =
      marianatrench::constants::get_dfa_annotation_type();
  for (const auto& annotation : clazz->get_anno_set()->get_annotations()) {
    if (!annotation->type() ||
        annotation->type()->str() != dfa_annotation_type) {
      continue;
    }

    auto public_scope = marianatrench::constants::get_public_access_scope();
    for (const DexAnnotationElement& element : annotation->anno_elems()) {
      if (element.string->str() == "enforceScope" &&
          element.encoded_value->as_value() == 0) {
        return true;
      }

      if (element.string->str() == "accessScope" &&
          element.encoded_value->show() == public_scope) {
        return true;
      }
    }
  }
  return false;
}
} // namespace

DFASourceGenerator::DFASourceGenerator(Context& context)
    : ModelGenerator("dfa_source_generator", context) {}

std::vector<Model> DFASourceGenerator::emit_method_models(
    const Methods& methods) {
  ConcurrentSet<const DexClass*> dfa_classes;
  for (auto& scope : DexStoreClassesIterator(context_.stores)) {
    walk::parallel::classes(scope, [&dfa_classes](DexClass* clazz) {
      if (is_class_accessible_via_dfa(clazz)) {
        dfa_classes.emplace(clazz);
      }
    });
  }

  ConcurrentSet<const DexClass*> nested_dfa_classes;
  for (auto& scope : DexStoreClassesIterator(context_.stores)) {
    walk::parallel::classes(
        scope, [&dfa_classes, &nested_dfa_classes](DexClass* clazz) {
          for (const DexClass* exported_class :
               UnorderedIterable(dfa_classes)) {
            auto dex_klass_prefix = exported_class->get_name()->str_copy();
            dex_klass_prefix.erase(dex_klass_prefix.length() - 1);

            if (boost::starts_with(show(clazz), dex_klass_prefix)) {
              nested_dfa_classes.emplace(clazz);
            }
          }
        });
  }

  insert_unordered_iterable(dfa_classes, nested_dfa_classes);

  std::vector<Model> models;
  for (const auto* dex_klass : UnorderedIterable(dfa_classes)) {
    // Mark all public and protected methods in the class as exported.
    for (const auto* dex_callee : dex_klass->get_all_methods()) {
      if (dex_callee == nullptr) {
        continue;
      }

      if (dex_callee->get_access() & DexAccessFlags::ACC_PRIVATE) {
        continue;
      }

      const auto* callee = methods.get(dex_callee);
      if (callee == nullptr) {
        continue;
      }

      Model model(callee, context_);
      model.add_call_effect_source(
          AccessPath(Root(Root::Kind::CallEffectExploitability)),
          generator::source(
              context_,
              /* kind */ "DfaComponent"),
          *context_.heuristics);
      models.push_back(model);
    }
  }
  return models;
}

} // namespace marianatrench

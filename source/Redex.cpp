/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <json/json.h>

#include <Creators.h>
#include <DexAccess.h>
#include <DexClass.h>
#include <DexUtil.h>
#include <IRAssembler.h>
#include <ProguardConfiguration.h>
#include <ProguardMap.h>
#include <ProguardMatcher.h>
#include <ProguardParser.h>
#include <Reachability.h>
#include <Resolver.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Redex.h>

namespace marianatrench {

DexClass* redex::get_class(const std::string& class_name) {
  auto* type = get_type(class_name);
  if (type) {
    return type_class(type);
  }
  return nullptr;
}

DexMethod* redex::get_method(const std::string& signature) {
  auto method_reference = DexMethod::get_method(signature);
  if (!method_reference) {
    return nullptr;
  }
  return method_reference->as_def();
}

DexFieldRef* redex::get_field(const std::string& field) {
  return DexField::get_field(field);
}

DexType* redex::get_type(const std::string& type) {
  return DexType::get_type(type);
}

void redex::remove_unreachable(
    const std::vector<std::string>& proguard_configuration_paths,
    DexStoresVector& stores,
    const std::optional<boost::filesystem::path>& removed_symbols_path) {
  keep_rules::ProguardConfiguration proguard_configuration;

  if (proguard_configuration_paths.size() == 0) {
    return;
  }

  for (const auto& proguard_configuration_path : proguard_configuration_paths) {
    keep_rules::proguard_parser::parse_file(
        proguard_configuration_path, &proguard_configuration);
  }

  ProguardMap empty_map;

  for (auto& store : stores) {
    apply_deobfuscated_names(store.get_dexen(), empty_map);
  }

  keep_rules::process_proguard_rules(
      empty_map,
      build_class_scope(stores),
      g_redex->external_classes(),
      proguard_configuration,
      false);

  auto reachables = reachability::compute_reachable_objects(
      stores,
      /* empty ignore sets */ reachability::IgnoreSets(),
      /* number of ignore check strings */ nullptr,
      /* emit_graph_this_run */ false);

  reachability::ObjectCounts before = reachability::count_objects(stores);
  LOG(1,
      "Removing unreachable code in {} classes, {} fields, {} methods.",
      before.num_classes,
      before.num_fields,
      before.num_methods);

  ConcurrentSet<std::string> removed_symbols;
  reachability::sweep(
      stores, *reachables, removed_symbols_path ? &removed_symbols : nullptr);

  if (removed_symbols_path) {
    auto value = Json::Value(Json::arrayValue);
    for (const auto& symbol : removed_symbols) {
      value.append(symbol);
    }
    JsonValidation::write_json_file(*removed_symbols_path, value);
  }

  reachability::ObjectCounts after = reachability::count_objects(stores);
  LOG(1,
      "Unreachables removed. {} classes, {} fields, {} methods are left.",
      after.num_classes,
      after.num_fields,
      after.num_methods);
}

std::vector<DexMethod*> redex::create_methods(
    Scope& scope,
    const std::string& class_name,
    const std::vector<std::string>& bodies,
    const DexType* super,
    const std::optional<std::vector<std::string>>& annotations) {
  std::vector<DexMethod*> dex_methods;

  auto* type = DexType::make_type(DexString::make_string(class_name));
  ClassCreator creator(type);

  if (super) {
    creator.set_super(const_cast<DexType*>(super));
  } else {
    creator.set_super(type::java_lang_Object());
  }

  for (const auto& body : bodies) {
    auto* dex_method = assembler::method_from_string(body);
    if (annotations) {
      dex_method->make_non_concrete();
      dex_method->set_external();
      dex_method->attach_annotation_set(create_annotation_set(*annotations));
    }
    dex_methods.push_back(dex_method);
    creator.add_method(dex_method);
  }

  scope.push_back(creator.create());

  return dex_methods;
}

DexMethod* redex::create_method(
    Scope& scope,
    const std::string& class_name,
    const std::string& body,
    const DexType* super,
    const std::optional<std::vector<std::string>>& annotations) {
  return create_methods(scope, class_name, {body}, super, annotations).front();
}

DexMethod* redex::create_void_method(
    Scope& scope,
    const std::string& class_name,
    const std::string& method_name,
    const std::string& parameter_types,
    const std::string& return_type,
    const DexType* super,
    bool is_static,
    bool is_private,
    bool is_native,
    const std::optional<std::vector<std::string>>& annotations) {
  std::string access = is_private ? "private" : "public";
  if (is_static) {
    access.append(" static");
  }
  if (is_native) {
    access.append(" native");
  }
  auto body = fmt::format(
      R"(
        (method ({}) "{}.{}:({}){}"
         (
          (return-void)
         )
        )
      )",
      access,
      class_name,
      method_name,
      parameter_types,
      return_type);
  auto* dex_method = create_method(scope, class_name, body, super, annotations);

  // Sanity checks
  if (!dex_method->is_external()) {
    mt_assert(::is_static(dex_method) == is_static);
    mt_assert(::is_private(dex_method) == is_private);
    mt_assert(::is_public(dex_method) == !is_private);
    mt_assert(::is_native(dex_method) == is_native);
  }

  return dex_method;
}

DexAnnotationSet* redex::create_annotation_set(
    const std::vector<std::string>& annotations) {
  DexAnnotationSet* dannoset = new DexAnnotationSet();

  for (const std::string& anno : annotations) {
    DexString* dstring = DexString::make_string(anno);
    DexType* dtype = DexType::make_type(dstring);
    DexAnnotation* danno =
        new DexAnnotation(dtype, DexAnnotationVisibility::DAV_RUNTIME);
    dannoset->add_annotation(danno);
  }

  return dannoset;
}

} // namespace marianatrench

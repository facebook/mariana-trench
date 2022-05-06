/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
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
#include <RedexContext.h>
#include <Resolver.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Redex.h>

namespace marianatrench {

DexClass* redex::get_class(std::string_view class_name) {
  auto* type = get_type(class_name);
  if (type) {
    return type_class(type);
  }
  return nullptr;
}

DexMethod* redex::get_method(std::string_view signature) {
  auto method_reference = DexMethod::get_method(signature);
  if (!method_reference) {
    return nullptr;
  }
  return method_reference->as_def();
}

DexFieldRef* redex::get_field(std::string_view field) {
  return DexField::get_field(field);
}

DexType* redex::get_type(std::string_view type) {
  return DexType::get_type(type);
}

void redex::process_proguard_configurations(
    const Options& options,
    const DexStoresVector& stores) {
  /* Parse and register proguard configuration contents to redex global state.
  Used in global type analysis and removing unreachable paths. */
  const std::vector<std::string>& proguard_configuration_paths =
      options.proguard_configuration_paths();

  if (proguard_configuration_paths.empty()) {
    return;
  }

  keep_rules::ProguardConfiguration proguard_configuration;
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
}

void redex::remove_unreachable(
    const Options& options,
    DexStoresVector& stores) {
  const std::vector<std::string>& proguard_configuration_paths =
      options.proguard_configuration_paths();
  const std::optional<boost::filesystem::path>& removed_symbols_path =
      options.removed_symbols_output_path();
  keep_rules::ProguardConfiguration proguard_configuration;

  if (proguard_configuration_paths.empty()) {
    return;
  }

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

DexClass* redex::create_class(
    Scope& scope,
    const std::string& class_name,
    const DexType* super) {
  auto* type = DexType::make_type(DexString::make_string(class_name));
  ClassCreator creator(type);

  if (super) {
    creator.set_super(const_cast<DexType*>(super));
  } else {
    creator.set_super(type::java_lang_Object());
  }

  auto* klass = creator.create();
  scope.push_back(klass);

  return klass;
}

std::vector<DexMethod*> redex::create_methods(
    Scope& scope,
    const std::string& class_name,
    const std::vector<DexMethodSpecification>& methods,
    const DexType* super) {
  std::vector<DexMethod*> dex_methods;

  auto* type = DexType::make_type(DexString::make_string(class_name));
  ClassCreator creator(type);

  if (super) {
    creator.set_super(const_cast<DexType*>(super));
  } else {
    creator.set_super(type::java_lang_Object());
  }

  for (const auto& method : methods) {
    auto* dex_method = assembler::method_from_string(method.body);
    if (!method.annotations.empty()) {
      dex_method->make_non_concrete();
      dex_method->set_external();
      dex_method->attach_annotation_set(
          create_annotation_set(method.annotations));
    }
    if (method.abstract) {
      dex_method->set_code(nullptr);
    }
    dex_methods.push_back(dex_method);
    creator.add_method(dex_method);
  }
  scope.push_back(creator.create());

  return dex_methods;
}

std::vector<DexMethod*> redex::create_methods(
    Scope& scope,
    const std::string& class_name,
    const std::vector<std::string>& bodies,
    const DexType* super) {
  std::vector<DexMethodSpecification> methods;
  for (const auto& body : bodies) {
    methods.push_back(DexMethodSpecification{body});
  }
  return create_methods(scope, class_name, methods, super);
}

DexMethod* redex::create_method(
    Scope& scope,
    const std::string& class_name,
    const std::string& body,
    const DexType* super,
    const bool abstract,
    const std::vector<std::string>& annotations) {
  auto method = DexMethodSpecification{
      body,
      abstract,
      annotations,
  };
  return create_methods(scope, class_name, {method}, super).front();
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
    bool is_abstract,
    const std::vector<std::string>& annotations) {
  std::string access = is_private ? "private" : "public";
  if (is_static) {
    access.append(" static");
  }
  if (is_native) {
    access.append(" native");
  }
  std::string return_statement = "(return-void)";
  if (return_type != "V") {
    return_statement =
        R"(
          (new-instance "Ljava/lang/Object;")
          (move-result-pseudo-object v0)
          (return-object v0)
        )";
  }
  auto body = fmt::format(
      R"(
        (method ({}) "{}.{}:({}){}"
         (
          {}
         )
        )
      )",
      access,
      class_name,
      method_name,
      parameter_types,
      return_type,
      return_statement);
  auto* dex_method =
      create_method(scope, class_name, body, super, is_abstract, annotations);

  // Sanity checks
  if (!dex_method->is_external()) {
    mt_assert(::is_static(dex_method) == is_static);
    mt_assert(::is_private(dex_method) == is_private);
    mt_assert(::is_public(dex_method) == !is_private);
    mt_assert(::is_native(dex_method) == is_native);
  }

  return dex_method;
}

std::unique_ptr<DexAnnotationSet> redex::create_annotation_set(
    const std::vector<std::string>& annotations) {
  auto dannoset = std::make_unique<DexAnnotationSet>();

  for (const std::string& anno : annotations) {
    const DexString* dstring = DexString::make_string(anno);
    DexType* dtype = DexType::make_type(dstring);
    dannoset->add_annotation(std::make_unique<DexAnnotation>(
        dtype, DexAnnotationVisibility::DAV_RUNTIME));
  }

  return dannoset;
}

const DexField* redex::create_field(
    Scope& scope,
    const std::string& class_name,
    const DexFieldSpecification& field,
    const DexType* super,
    bool is_static) {
  return redex::create_fields(scope, class_name, {field}, super, is_static)[0];
}

std::vector<const DexField*> redex::create_fields(
    Scope& scope,
    const std::string& class_name,
    const std::vector<DexFieldSpecification>& fields,
    const DexType* super,
    bool is_static) {
  std::vector<const DexField*> created_fields;
  auto* klass = DexType::make_type(DexString::make_string(class_name));
  ClassCreator creator(klass);

  if (super) {
    creator.set_super(const_cast<DexType*>(super));
  } else {
    creator.set_super(type::java_lang_Object());
  }

  for (const auto& [field_name, field_type, field_annotations] : fields) {
    // Cast to DexField so that we can add annotations to it
    auto* field = static_cast<DexField*>(DexField::make_field(
        /* container */ klass,
        /* name */ DexString::make_string(field_name),
        /* type */ field_type));
    field->attach_annotation_set(
        redex::create_annotation_set(field_annotations));
    auto* concrete_field = field->make_concrete(
        is_static ? DexAccessFlags::ACC_STATIC : DexAccessFlags::ACC_PUBLIC,
        nullptr);
    creator.add_field(concrete_field);
    created_fields.push_back(concrete_field);
  }

  scope.push_back(creator.create());
  return created_fields;
}

} // namespace marianatrench

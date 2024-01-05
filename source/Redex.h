/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <filesystem>
#include <optional>

#include <DexClass.h>
#include <DexStore.h>

#include <mariana-trench/Compiler.h>
#include <mariana-trench/Options.h>

namespace marianatrench {

namespace redex {

struct DexMethodSpecification {
  std::string body;
  bool abstract = false;
  std::vector<std::string> annotations = {};
};

struct DexFieldSpecification {
  std::string field_name;
  const DexType* field_type;
  std::vector<std::string> annotations = {};
};

DexClass* MT_NULLABLE get_class(std::string_view class_name);

DexMethod* MT_NULLABLE get_method(std::string_view signature);

DexFieldRef* MT_NULLABLE get_field(std::string_view field);

DexType* MT_NULLABLE get_type(std::string_view type);

void process_proguard_configurations(
    const Options& options,
    const DexStoresVector& stores);

void remove_unreachable(const Options& options, DexStoresVector& stores);

DexClass* create_class(
    Scope& scope,
    const std::string& class_name,
    const DexType* super = nullptr);

DexClass* create_methods_and_fields(
    Scope& scope,
    const std::string& class_name,
    const std::vector<DexMethodSpecification>& bodies,
    const std::vector<DexFieldSpecification>& fields,
    const DexType* super = nullptr,
    bool fields_are_static = false);

std::vector<DexMethod*> create_methods(
    Scope& scope,
    const std::string& class_name,
    const std::vector<DexMethodSpecification>& bodies,
    const DexType* super);

std::vector<DexMethod*> create_methods(
    Scope& scope,
    const std::string& class_name,
    const std::vector<std::string>& bodies,
    const DexType* super = nullptr);

DexMethod* create_method(
    Scope& scope,
    const std::string& class_name,
    const std::string& body,
    const DexType* super = nullptr,
    const bool abstract = false,
    const std::vector<std::string>& annotations = std::vector<std::string>());

DexMethod* create_void_method(
    Scope& scope,
    const std::string& class_name,
    const std::string& method_name,
    const std::string& parameter_types = "",
    const std::string& return_type = "V",
    const DexType* super = nullptr,
    bool is_static = false,
    bool is_private = false,
    bool is_native = false,
    bool is_abstract = false,
    const std::vector<std::string>& annotations = std::vector<std::string>());

std::unique_ptr<DexAnnotationSet> create_annotation_set(
    const std::vector<std::string>& annotations,
    std::optional<std::string> element = std::nullopt);

const DexField* create_field(
    Scope& scope,
    const std::string& class_name,
    const DexFieldSpecification& field,
    const DexType* super = nullptr,
    bool is_static = false);

std::vector<const DexField*> create_fields(
    Scope& scope,
    const std::string& class_name,
    const std::vector<DexFieldSpecification>& fields,
    const DexType* super = nullptr,
    bool is_static = false);

std::optional<DexMethodSpec> get_method_spec(std::string_view signature);

} // namespace redex

} // namespace marianatrench

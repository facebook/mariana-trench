/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>

#include <boost/filesystem.hpp>

#include <DexClass.h>
#include <DexStore.h>

#include <mariana-trench/Compiler.h>
#include <mariana-trench/Options.h>

namespace marianatrench {

namespace redex {

struct DexMethodSpecification {
  std::string body;
  bool abstract = false;
  std::vector<std::string> annotations = std::vector<std::string>();
};

DexClass* MT_NULLABLE get_class(const std::string& class_name);

DexMethod* MT_NULLABLE get_method(const std::string& signature);

DexFieldRef* MT_NULLABLE get_field(const std::string& field);

DexType* MT_NULLABLE get_type(const std::string& type);

void process_proguard_configurations(
    const Options& options,
    const DexStoresVector& stores);

void remove_unreachable(const Options& options, DexStoresVector& stores);

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

DexAnnotationSet* create_annotation_set(
    const std::vector<std::string>& annotations);

std::vector<const DexField*> create_fields(
    Scope& scope,
    const std::string& class_name,
    const std::vector<std::pair<std::string, const DexType*>>& fields,
    const DexType* super = nullptr);

} // namespace redex

} // namespace marianatrench

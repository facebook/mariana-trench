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

namespace marianatrench {

namespace redex {

DexClass* MT_NULLABLE get_class(const std::string& class_name);

DexMethod* MT_NULLABLE get_method(const std::string& signature);

DexFieldRef* MT_NULLABLE get_field(const std::string& field);

DexType* MT_NULLABLE get_type(const std::string& type);

void remove_unreachable(
    const std::vector<std::string>& proguard_configuration_paths,
    DexStoresVector& stores,
    const std::optional<boost::filesystem::path>& removed_symbols_path =
        std::nullopt);

std::vector<DexMethod*> create_methods(
    Scope& scope,
    const std::string& class_name,
    const std::vector<std::string>& bodies,
    const DexType* super = nullptr,
    const std::optional<std::vector<std::string>>& annotations = std::nullopt);

DexMethod* create_method(
    Scope& scope,
    const std::string& class_name,
    const std::string& body,
    const DexType* super = nullptr,
    const std::optional<std::vector<std::string>>& annotations = std::nullopt);

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
    const std::optional<std::vector<std::string>>& annotations = std::nullopt);

DexAnnotationSet* create_annotation_set(
    const std::vector<std::string>& annotations);

} // namespace redex

} // namespace marianatrench

/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>
#include <string>

#include <DexClass.h>
#include <DexUtil.h>
#include <RedexResources.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/Options.h>

namespace marianatrench {

class Generator {
 public:
  Generator(const std::string& name, Context& context);

  Generator(const Generator&) = delete;
  Generator(Generator&&) = default;
  Generator& operator=(const Generator&) = delete;
  Generator& operator=(Generator&&) = delete;
  virtual ~Generator() = default;

  virtual const std::string& name() const {
    return name_;
  }

  virtual std::vector<Model> run(const DexStoresVector& stores) = 0;

 protected:
  std::string name_;
  Context& context_;
  const Options& options_;
  const Methods& methods_;
  const Overrides& overrides_;
};

class MethodVisitorGenerator : public Generator {
 public:
  MethodVisitorGenerator(const std::string& name, Context& context)
      : Generator(name, context) {}

  virtual std::vector<Model> run(const DexStoresVector& stores) override;

  // This method has to be thread-safe.
  virtual std::vector<Model> visit_method(const Method* method) const = 0;
};

namespace generator {

const std::string& get_class_name(const Method* method);
const std::string& get_method_name(const Method* method);
std::optional<std::string> get_super_type(const Method* method);
std::optional<const DexType*> get_return_type(const Method* method);
std::optional<std::string> get_return_type_string(const Method* method);
std::unordered_set<std::string> get_custom_parents_from_class(
    DexClass* dex_class);
std::string get_outer_class(const std::string& classname);

bool is_numeric_data_type(const DataType& type);

std::vector<std::pair<ParameterPosition, const DexType*>> get_argument_types(
    const DexMethod* dex_method);
std::vector<std::pair<ParameterPosition, const DexType*>> get_argument_types(
    const Method* method);
std::vector<std::pair<ParameterPosition, std::string>>
get_argument_types_string(const Method* method);

void add_propagation_to_return(
    Context& context,
    Model& model,
    ParameterPosition parameter_position,
    const std::vector<std::string>& features = {});
void add_propagation_to_parameter(
    Context& context,
    Model& model,
    ParameterPosition from,
    ParameterPosition to,
    const std::vector<std::string>& features = {});
void add_propagation_to_self(
    Context& context,
    Model& model,
    ParameterPosition parameter_position,
    const std::vector<std::string>& features = {});

/* Checks whether the given method is annotated with the given annotation type
 * and value. */
bool has_annotation(
    const DexMethod* method,
    const std::string& expected_type,
    const std::optional<std::unordered_set<std::string>>& expected_values =
        std::nullopt);

/* Checks whether the given class is annotated with the given annotation type
 * and value. */
bool has_annotation(
    const DexClass* dex_class,
    const std::string& expected_type,
    const std::optional<std::unordered_set<std::string>>& expected_values =
        std::nullopt);

Frame source(
    Context& context,
    const Method* method,
    const std::string& kind,
    const std::vector<std::string>& features = {},
    Root::Kind callee_port = Root::Kind::Leaf);
Frame sink(
    Context& context,
    const Method* method,
    const std::string& kind,
    const std::vector<std::string>& features = {},
    Root::Kind callee_port = Root::Kind::Leaf);

} // namespace generator
} // namespace marianatrench

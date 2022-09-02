/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
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
#include <Walkers.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/FieldModel.h>
#include <mariana-trench/Frame.h>
#include <mariana-trench/MethodMappings.h>
#include <mariana-trench/MethodSet.h>
#include <mariana-trench/Methods.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Overrides.h>

namespace marianatrench {

struct ModelGeneratorResult {
  std::vector<Model> method_models;
  std::vector<FieldModel> field_models;
};

class ModelGenerator {
 public:
  ModelGenerator(const std::string& name, Context& context);

  ModelGenerator(const ModelGenerator&) = delete;
  ModelGenerator(ModelGenerator&&) = default;
  ModelGenerator& operator=(const ModelGenerator&) = delete;
  ModelGenerator& operator=(ModelGenerator&&) = delete;
  virtual ~ModelGenerator() = default;

  virtual const std::string& name() const {
    return name_;
  }

  virtual std::vector<Model> emit_method_models(const Methods& /* methods */) {
    return {};
  }

  virtual std::vector<Model> emit_method_models_optimized(
      const Methods& methods,
      const MethodMappings& method_mappings);

  virtual std::vector<FieldModel> emit_field_models(
      const Fields& /* fields */) {
    return {};
  }

  ModelGeneratorResult run(const Methods& methods, const Fields& fields);
  ModelGeneratorResult run_optimized(
      const Methods& methods,
      const MethodMappings& method_mappings,
      const Fields& fields);

 protected:
  std::string name_;
  Context& context_;
  const Options& options_;
  const Methods& methods_;
  const Overrides& overrides_;
};

class MethodVisitorModelGenerator : public ModelGenerator {
 public:
  MethodVisitorModelGenerator(const std::string& name, Context& context)
      : ModelGenerator(name, context) {}

  virtual std::vector<Model> emit_method_models(const Methods&) override;

  // This method has to be thread-safe.
  virtual std::vector<Model> visit_method(const Method* method) const = 0;

 protected:
  template <typename InputIt>
  std::vector<Model> run_impl(InputIt begin, InputIt end) {
    std::vector<Model> models;
    std::mutex mutex;

    auto queue = sparta::work_queue<const Method*>([&](const Method* method) {
      std::vector<Model> method_models = this->visit_method(method);

      if (!method_models.empty()) {
        std::lock_guard<std::mutex> lock(mutex);
        models.insert(
            models.end(),
            std::make_move_iterator(method_models.begin()),
            std::make_move_iterator(method_models.end()));
      }
    });
    for (auto iterator = begin; iterator != end; ++iterator) {
      queue.add_item(*iterator);
    }
    queue.run_all();
    return models;
  }
};

class FieldVisitorModelGenerator : public ModelGenerator {
 public:
  FieldVisitorModelGenerator(const std::string& name, Context& context)
      : ModelGenerator(name, context) {}

  virtual std::vector<FieldModel> emit_field_models(const Fields&) override;

  // This method has to be thread-safe.
  virtual std::vector<FieldModel> visit_field(const Field* field) const = 0;

 protected:
  template <typename InputIt>
  std::vector<FieldModel> run_impl(InputIt begin, InputIt end) {
    std::vector<FieldModel> models;
    std::mutex mutex;

    auto queue = sparta::work_queue<const Field*>([&](const Field* field) {
      auto field_models = this->visit_field(field);

      if (!field_models.empty()) {
        std::lock_guard<std::mutex> lock(mutex);
        models.insert(
            models.end(),
            std::make_move_iterator(field_models.begin()),
            std::make_move_iterator(field_models.end()));
      }
    });
    for (auto iterator = begin; iterator != end; ++iterator) {
      queue.add_item(*iterator);
    }
    queue.run_all();
    return models;
  }
};

namespace generator {

std::string_view get_class_name(const Method* method);
std::string_view get_method_name(const Method* method);
std::optional<std::string_view> get_super_type(const Method* method);
std::optional<const DexType*> get_return_type(const Method* method);
std::optional<std::string_view> get_return_type_string(const Method* method);
std::unordered_set<std::string_view> get_interfaces_from_class(
    DexClass* dex_class);
std::unordered_set<std::string_view> get_parents_from_class(
    DexClass* dex_class,
    bool include_interfaces);
std::unordered_set<std::string_view> get_custom_parents_from_class(
    DexClass* dex_class);
std::string get_outer_class(std::string_view classname);

bool is_numeric_data_type(const DataType& type);

std::vector<std::pair<ParameterPosition, const DexType*>> get_argument_types(
    const DexMethod* dex_method);
std::vector<std::pair<ParameterPosition, const DexType*>> get_argument_types(
    const Method* method);
std::vector<std::pair<ParameterPosition, std::string_view>>
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

TaintBuilder source(
    Context& context,
    const Method* method,
    const std::string& kind,
    const std::vector<std::string>& features = {},
    Root::Kind callee_port = Root::Kind::Leaf,
    RootSetAbstractDomain via_type_of_ports = {},
    RootSetAbstractDomain via_value_of_ports = {});
TaintBuilder sink(
    Context& context,
    const Method* method,
    const std::string& kind,
    const std::vector<std::string>& features = {},
    Root::Kind callee_port = Root::Kind::Leaf,
    RootSetAbstractDomain via_type_of_ports = {},
    RootSetAbstractDomain via_value_of_ports = {},
    CanonicalNameSetAbstractDomain canonical_names =
        CanonicalNameSetAbstractDomain{});
TaintBuilder partial_sink(
    Context& context,
    const Method* method,
    const std::string& kind,
    const std::string& label,
    const std::vector<std::string>& features = {},
    Root::Kind callee_port = Root::Kind::Leaf,
    RootSetAbstractDomain via_type_of_ports = {},
    RootSetAbstractDomain via_value_of_ports = {});

} // namespace generator
} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Context.h>
#include <mariana-trench/Model.h>
#include <mariana-trench/constraints/FieldConstraints.h>
#include <mariana-trench/constraints/MethodConstraints.h>
#include <mariana-trench/model-generator/FieldModelTemplate.h>
#include <mariana-trench/model-generator/ModelGenerator.h>
#include <mariana-trench/model-generator/ModelTemplates.h>

namespace marianatrench {

/* This class parses a json format model generator and stores its information */
class JsonModelGeneratorItem final : public MethodVisitorModelGenerator {
 public:
  JsonModelGeneratorItem(
      const std::string& name,
      Context& context,
      std::unique_ptr<AllOfMethodConstraint> constraint,
      ModelTemplate model_template,
      int verbosity);
  std::vector<Model> emit_method_models_filtered(
      const MethodHashedSet& methods);
  std::unordered_set<const MethodConstraint*> constraint_leaves() const;
  /* Returns filtered method set to run full satisfy checks on. Returns Top if
   * filtered set cannot be determined. */
  MethodHashedSet may_satisfy(const MethodMappings& method_mappings) const;
  std::vector<Model> visit_method(const Method* method) const override;

 private:
  std::unique_ptr<AllOfMethodConstraint> constraint_;
  ModelTemplate model_template_;
  int verbosity_;
};

class JsonFieldModelGeneratorItem final : public FieldVisitorModelGenerator {
 public:
  explicit JsonFieldModelGeneratorItem(
      const std::string& name,
      Context& context,
      std::unique_ptr<AllOfFieldConstraint> constraint,
      FieldModelTemplate field_model_template,
      int verbosity);
  std::vector<FieldModel> visit_field(const Field* field) const override;

 private:
  std::unique_ptr<AllOfFieldConstraint> constraint_;
  FieldModelTemplate field_model_template_;
  int verbosity_;
};

class JsonModelGenerator final : public ModelGenerator {
 public:
  explicit JsonModelGenerator(
      const std::string& name,
      Context& context,
      const boost::filesystem::path& json_configuration_file);

  std::vector<Model> emit_method_models(const Methods&) override;
  std::vector<Model> emit_method_models_optimized(
      const Methods&,
      const MethodMappings& method_mappings) override;
  std::vector<FieldModel> emit_field_models(const Fields&) override;

 private:
  boost::filesystem::path json_configuration_file_;
  std::vector<JsonModelGeneratorItem> items_;
  std::vector<JsonFieldModelGeneratorItem> field_items_;
};

} // namespace marianatrench

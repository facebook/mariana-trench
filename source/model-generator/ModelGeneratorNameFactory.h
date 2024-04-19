/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ConcurrentContainers.h>

#include <mariana-trench/Compiler.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/model-generator/ModelGeneratorName.h>

namespace marianatrench {

class ModelGeneratorNameFactory final {
 public:
  ModelGeneratorNameFactory();

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(ModelGeneratorNameFactory)

  const ModelGeneratorName* create(const std::string& identifier) const;

  const ModelGeneratorName* create(
      const std::string& identifier,
      const std::string& part) const;

  const ModelGeneratorName* create(const std::string& identifier, int part)
      const;

  // A "sharded" model generator name refers to models that were created as a
  // result of reading in models from `Options::sharded_models_directory()`.
  // An empty `original_generator` refers to the model being parsed from the
  // sharded models directory but not necesarily originating from any
  // user-defined model generator, i.e. it could have been an inferred model.
  const ModelGeneratorName* create_sharded(
      const std::string& identifier,
      const ModelGeneratorName* original_generator) const;

  const ModelGeneratorName* create(
      const std::string& identifier,
      std::optional<std::string> part,
      bool is_sharded) const;

  const ModelGeneratorName* MT_NULLABLE
  get(const std::string& identifier) const;

  static const ModelGeneratorNameFactory& singleton();

 private:
  mutable InsertOnlyConcurrentSet<ModelGeneratorName> set_;
};

} // namespace marianatrench

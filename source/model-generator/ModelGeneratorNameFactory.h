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

  const ModelGeneratorName* MT_NULLABLE
  get(const std::string& identifier) const;

  static const ModelGeneratorNameFactory& singleton();

 private:
  mutable InsertOnlyConcurrentSet<ModelGeneratorName> set_;
};

} // namespace marianatrench

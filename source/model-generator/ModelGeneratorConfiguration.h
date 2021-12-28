/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>

#include <json/json.h>

namespace marianatrench {

class ModelGeneratorConfiguration {
 public:
  explicit ModelGeneratorConfiguration(const std::string& name);

  static ModelGeneratorConfiguration from_json(const Json::Value& value);

  const std::string& name() const;

 private:
  std::string name_;
};
} // namespace marianatrench

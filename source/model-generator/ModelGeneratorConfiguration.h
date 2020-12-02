// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <string>

#include <json/json.h>

namespace marianatrench {

class ModelGeneratorConfiguration {
 public:
  enum class Kind { JSON, CPP };

  explicit ModelGeneratorConfiguration(Kind kind, std::string name_or_path);

  static ModelGeneratorConfiguration from_json(const Json::Value& value);

  ModelGeneratorConfiguration::Kind kind() const;
  const std::string& name_or_path() const;

 private:
  ModelGeneratorConfiguration::Kind kind_;
  std::string name_or_path_;
};
} // namespace marianatrench

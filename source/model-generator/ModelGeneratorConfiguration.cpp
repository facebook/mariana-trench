// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#include <mariana-trench/model-generator/ModelGeneratorConfiguration.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Log.h>

namespace marianatrench {

ModelGeneratorConfiguration::ModelGeneratorConfiguration(
    ModelGeneratorConfiguration::Kind kind,
    std::string name_or_path)
    : kind_(kind), name_or_path_(name_or_path) {}

ModelGeneratorConfiguration ModelGeneratorConfiguration::from_json(
    const Json::Value& value) {
  JsonValidation::validate_object(value);

  const std::string kind = JsonValidation::string(value, "kind");
  if (kind == "json") {
    return ModelGeneratorConfiguration(
        ModelGeneratorConfiguration::Kind::JSON,
        JsonValidation::string(value, "path"));
  } else if (kind == "cpp") {
    return ModelGeneratorConfiguration(
        ModelGeneratorConfiguration::Kind::CPP,
        JsonValidation::string(value, "name"));
  } else {
    throw JsonValidationError(
        value,
        /* field */ "kind",
        /* expected */ "cpp or json");
  }
}

ModelGeneratorConfiguration::Kind ModelGeneratorConfiguration::kind() const {
  return kind_;
}

const std::string& ModelGeneratorConfiguration::name_or_path() const {
  return name_or_path_;
}

} // namespace marianatrench

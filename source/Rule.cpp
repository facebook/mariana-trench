/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Kinds.h>
#include <mariana-trench/MultiSourceMultiSinkRule.h>
#include <mariana-trench/Rule.h>
#include <mariana-trench/SourceSinkRule.h>

namespace marianatrench {

std::unique_ptr<Rule> Rule::from_json(
    const Json::Value& value,
    Context& context) {
  JsonValidation::validate_object(value);

  auto name = JsonValidation::string(value, /* field */ "name");
  int code = JsonValidation::integer(value, /* field */ "code");
  auto description = JsonValidation::string(value, /* field */ "description");

  // This uses the presence of specific keys to determine the rule kind.
  // Unfortunately, it means users can write ambiguous nonsense without being
  // warned that a certain field is meaningless, such as:
  //   "sources": [...], "sinks": [...], "partial_sinks": [...]
  if (value.isMember("sources") && value.isMember("sinks")) {
    return SourceSinkRule::from_json(name, code, description, value, context);
  } else if (
      value.isMember("multi_sources") && value.isMember("partial_sinks")) {
    return MultiSourceMultiSinkRule::from_json(
        name, code, description, value, context);
  }

  throw JsonValidationError(
      value,
      std::nullopt,
      "keys: sources+sinks or multi_sources+partial_sinks");
}

Json::Value Rule::to_json() const {
  auto value = Json::Value(Json::objectValue);
  value["name"] = name_;
  value["code"] = Json::Value(code_);
  value["description"] = description_;
  return value;
}

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <vector>

#include <json/json.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/Rule.h>
#include <mariana-trench/TransformList.h>

namespace marianatrench {

/**
 * Represents a rule that combines distinct rules, each identifying a
 * different flow, into a single issue code.
 *
 * Example JSON format:
 * {
 *   "name": "My Rule",
 *   "code": 4001,
 *   "description": "Description",
 *   "cases": [
 *     {
 *       "sources": ["SourceA"],
 *       "sinks": ["SinkX", "SinkY"],
 *       "transforms": ["T1"]
 *     },
 *     {
 *       "sources": ["SourceB"],
 *       "sinks": ["SinkZ"]
 *     }
 *   ]
 * }
 */
class MultiCaseRule final : public Rule {
 public:
  MultiCaseRule(
      const std::string& name,
      int code,
      const std::string& description,
      std::vector<std::unique_ptr<Rule>> cases)
      : Rule(name, code, description), cases_(std::move(cases)) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(MultiCaseRule)

  const std::vector<std::unique_ptr<Rule>>& cases() const {
    return cases_;
  }

  bool uses(const Kind*) const override;

  std::optional<CoveredRule> coverage(
      const KindSet& sources,
      const KindSet& sinks,
      const TransformSet& transforms) const override;

  static std::unique_ptr<Rule> from_json(
      const std::string& name,
      int code,
      const std::string& description,
      const Json::Value& value,
      Context& context);
  Json::Value to_json(bool include_metadata) const override;

 private:
  std::vector<std::unique_ptr<Rule>> cases_;
};

} // namespace marianatrench

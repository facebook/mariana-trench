/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/JsonValidation.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/Rule.h>
#include <mariana-trench/TransformList.h>

namespace marianatrench {

/**
 * Represents the typical source -> sink rule
 * e.g. UserControlled -> LaunchIntent
 * Optionally, transforms can be specified as a part of the rule which specifies
 * an ordered list of transform kinds that the source and/or sink goes through.
 * e.g. UserControlled -> T1 -> ... Tn -> LaunchIntent
 */
class SourceSinkRule final : public Rule {
 public:
  SourceSinkRule(
      const std::string& name,
      int code,
      const std::string& description,
      const KindSet& source_kinds,
      const KindSet& sink_kinds,
      const TransformList* transforms)
      : Rule(name, code, description),
        source_kinds_(source_kinds),
        sink_kinds_(sink_kinds),
        transforms_(transforms) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(SourceSinkRule)

  const KindSet& source_kinds() const {
    return source_kinds_;
  }

  const KindSet& sink_kinds() const {
    return sink_kinds_;
  }

  const TransformList* MT_NULLABLE transform_kinds() const {
    return transforms_;
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
  KindSet source_kinds_;
  KindSet sink_kinds_;
  const TransformList* MT_NULLABLE transforms_;
};

} // namespace marianatrench

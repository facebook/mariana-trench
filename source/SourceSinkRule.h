/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include <json/json.h>

#include <mariana-trench/Assert.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/Rule.h>

namespace marianatrench {

/**
 * Represents the typical source -> sink rule
 * e.g. UserControlled -> LaunchIntent
 */
class SourceSinkRule final : public Rule {
 public:
  SourceSinkRule(
      const std::string& name,
      int code,
      const std::string& description,
      const KindSet& source_kinds,
      const KindSet& sink_kinds)
      : Rule(name, code, description),
        source_kinds_(source_kinds),
        sink_kinds_(sink_kinds) {}
  SourceSinkRule(const SourceSinkRule&) = delete;
  SourceSinkRule(SourceSinkRule&&) = delete;
  SourceSinkRule& operator=(const SourceSinkRule&) = delete;
  SourceSinkRule& operator=(SourceSinkRule&&) = delete;
  ~SourceSinkRule() override = default;

  const KindSet& source_kinds() const {
    return source_kinds_;
  }

  const KindSet& sink_kinds() const {
    return sink_kinds_;
  }

  bool uses(const Kind*) const override;

  static std::unique_ptr<Rule> from_json(
      const std::string& name,
      int code,
      const std::string& description,
      const Json::Value& value,
      Context& context);
  Json::Value to_json() const override;

 private:
  KindSet source_kinds_;
  KindSet sink_kinds_;
};

} // namespace marianatrench

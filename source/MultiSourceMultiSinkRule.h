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

#include <mariana-trench/Context.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/PartialKind.h>
#include <mariana-trench/Rule.h>

namespace marianatrench {

/**
 * Rules where multiple sources flow into a sink
 * e.g. UserControlled + Implicit Intent -> Launch Intent
 */
class MultiSourceMultiSinkRule final : public Rule {
 public:
  using MultiSourceKindsByLabel = std::unordered_map<std::string, KindSet>;
  using PartialKindSet = std::unordered_set<const PartialKind*>;

  MultiSourceMultiSinkRule(
      const std::string& name,
      int code,
      const std::string& description,
      const MultiSourceKindsByLabel& multi_source_kinds,
      const PartialKindSet& partial_sink_kinds)
      : Rule(name, code, description),
        multi_source_kinds_(multi_source_kinds),
        partial_sink_kinds_(partial_sink_kinds) {}
  MultiSourceMultiSinkRule(const MultiSourceMultiSinkRule&) = delete;
  MultiSourceMultiSinkRule(MultiSourceMultiSinkRule&&) = delete;
  MultiSourceMultiSinkRule& operator=(const MultiSourceMultiSinkRule&) = delete;
  MultiSourceMultiSinkRule& operator=(MultiSourceMultiSinkRule&&) = delete;
  ~MultiSourceMultiSinkRule() override = default;

  const MultiSourceKindsByLabel& multi_source_kinds() const {
    return multi_source_kinds_;
  }

  const PartialKindSet& partial_sink_kinds() const {
    return partial_sink_kinds_;
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
  MultiSourceKindsByLabel multi_source_kinds_;
  PartialKindSet partial_sink_kinds_;
};

} // namespace marianatrench

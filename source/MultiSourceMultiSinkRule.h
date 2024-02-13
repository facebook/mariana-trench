/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
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
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Kind.h>
#include <mariana-trench/PartialKind.h>
#include <mariana-trench/Rule.h>

namespace marianatrench {

/**
 * Rules where multiple sources flow into a sink.
 * Supports exactly 2 sources(/sinks).
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
        partial_sink_kinds_(partial_sink_kinds) {
    // Currently, multi-source rules only support exactly 2 sources flowing
    // into 2 sinks. There can be > 2 partial sinks, but each they must come
    // in pairs (one for each source kind's label).
    mt_assert(multi_source_kinds.size() == 2);
    mt_assert(partial_sink_kinds.size() % 2 == 0);
  }

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(MultiSourceMultiSinkRule)

  const MultiSourceKindsByLabel& multi_source_kinds() const {
    return multi_source_kinds_;
  }

  PartialKindSet partial_sink_kinds(const std::string& label) const;

  bool uses(const Kind*) const override;

  KindSet used_sources(const KindSet& sources) const override;
  KindSet used_sinks(const KindSet& sinks) const override;

  // Transforms not supported in multi-source/sink rules.
  TransformSet transforms() const override {
    return TransformSet{};
  }
  TransformSet used_transforms(const TransformSet&) const override {
    // Should not be called since `transforms()` is always empty.
    mt_unreachable();
  }

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

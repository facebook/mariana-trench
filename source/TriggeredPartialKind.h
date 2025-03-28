/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ostream>
#include <string>

#include <json/json.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/MultiSourceMultiSinkRule.h>
#include <mariana-trench/PartialKind.h>

namespace marianatrench {

/**
 * Used to represent sinks in multi-source-multi-sink rules.
 * A partial sink becomes triggered when a matching source flows into its
 * counterpart partial sink at the callsite.
 *
 * `partial_kind` is the partial sink kind that is still pending a source flow
 * before the rule is considered satisfied/fulfilled.
 *
 * `rule` is the one which caused the creation of the `TriggeredPartialKind`.
 * Rules may re-use the partial sinks (with different sources), so it is
 * important to know which one was satisfied in the counterpart flow.
 */
class TriggeredPartialKind final : public Kind {
 public:
  explicit TriggeredPartialKind(const PartialKind* partial_kind, int rule_code)
      : partial_kind_(partial_kind), rule_code_(rule_code) {}

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(TriggeredPartialKind)

  void show(std::ostream&) const override;

  Json::Value to_json() const override;

  std::string to_trace_string() const override;

  const PartialKind* partial_kind() const {
    return partial_kind_;
  }

  int rule_code() const {
    return rule_code_;
  }

 private:
  const PartialKind* partial_kind_;
  int rule_code_;
};

} // namespace marianatrench

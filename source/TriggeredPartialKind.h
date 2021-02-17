/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <ostream>
#include <string>

#include <json/json.h>

#include <mariana-trench/Context.h>
#include <mariana-trench/PartialKind.h>

namespace marianatrench {

/**
 * Used to represent sinks in multi-source-multi-sink rules.
 * A partial sink becomes triggered when a matching source flows into either of
 * the 2 partial sinks at the callsite.
 */
class TriggeredPartialKind final : public Kind {
 public:
  explicit TriggeredPartialKind(const PartialKind* partial_kind)
      : partial_kind_(partial_kind) {}
  TriggeredPartialKind(const TriggeredPartialKind&) = delete;
  TriggeredPartialKind(TriggeredPartialKind&&) = delete;
  TriggeredPartialKind& operator=(const TriggeredPartialKind&) = delete;
  TriggeredPartialKind& operator=(TriggeredPartialKind&&) = delete;
  ~TriggeredPartialKind() override = default;

  void show(std::ostream&) const override;

  Json::Value to_json() const override;

  const PartialKind* partial_kind() const {
    return partial_kind_;
  }

 private:
  const PartialKind* partial_kind_;
};

} // namespace marianatrench

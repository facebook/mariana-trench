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
#include <mariana-trench/Kind.h>

namespace marianatrench {

/**
 * Used to represent sinks in multi-source-multi-sink rules.
 * A partial sink kind is one that requires more than one source to reach all
 * labels before it is identified as a flow. Each PartialKind contains only
 * one label. There should be another PartialSink with the same name but a
 * different label.
 *
 * E.g: callable(x: Partial[name, labelX], y: Partial[name, labelY])
 * On argument x, the label would be "labelX". The label must match that used
 * in the rule specifications' sources.
 */
class PartialKind final : public Kind {
 public:
  explicit PartialKind(std::string name, std::string label)
      : name_(std::move(name)), label_(std::move(label)) {}
  PartialKind(const PartialKind&) = delete;
  PartialKind(PartialKind&&) = delete;
  PartialKind& operator=(const PartialKind&) = delete;
  PartialKind& operator=(PartialKind&&) = delete;
  ~PartialKind() override = default;

  void show(std::ostream&) const override;

  static const PartialKind* from_json(
      const Json::Value& value,
      const std::string& partial_label,
      Context& context);
  std::string to_trace_string() const override;

  const auto& name() const {
    return name_;
  }

  const auto& label() const {
    return label_;
  }

  /**
   * The "other" kind is a counterpart of this if both share the name but have
   * different labels. If two kinds are counterparts of each other, together,
   * they form a "full" sink for a `MultiSourceMultiSinkRule`, e.g.:
   *   multi_sink(partial_kind_a, partial_kind_b)
   * The two partial kinds above are counterparts of each other.
   *
   * This comparison is the reason we do not currently support more than
   * 2 sources -> 2 sinks in a `MultiSourceMultiSinkRule`. It assumes there can
   * only be one other sink.
   */
  bool is_counterpart(const PartialKind* other) const;

 private:
  std::string name_;
  std::string label_;
};

} // namespace marianatrench

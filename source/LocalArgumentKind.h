/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Access.h>
#include <mariana-trench/Context.h>
#include <mariana-trench/PropagationKind.h>

namespace marianatrench {

/**
 * Represents the argument of a method.
 *
 * This is used to represent a propagation within the `Taint` representation.
 * This is also used to infer propagations in the backward analysis.
 */
class LocalArgumentKind final : public PropagationKind {
 public:
  explicit LocalArgumentKind(ParameterPosition parameter)
      : parameter_(parameter) {}
  LocalArgumentKind(const LocalArgumentKind&) = delete;
  LocalArgumentKind(LocalArgumentKind&&) = delete;
  LocalArgumentKind& operator=(const LocalArgumentKind&) = delete;
  LocalArgumentKind& operator=(LocalArgumentKind&&) = delete;
  ~LocalArgumentKind() override = default;

  void show(std::ostream&) const override;

  std::string to_trace_string() const override;

  Root root() const override;

 private:
  ParameterPosition parameter_;
};

} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Context.h>
#include <mariana-trench/PropagationKind.h>

namespace marianatrench {

/**
 * Represents the return value of a method.
 * This is used in the backward analysis to infer propagations.
 */
class LocalReturnKind final : public PropagationKind {
 public:
  explicit LocalReturnKind() {}
  LocalReturnKind(const LocalReturnKind&) = delete;
  LocalReturnKind(LocalReturnKind&&) = delete;
  LocalReturnKind& operator=(const LocalReturnKind&) = delete;
  LocalReturnKind& operator=(LocalReturnKind&&) = delete;
  ~LocalReturnKind() override = default;

  void show(std::ostream&) const override;

  std::string to_trace_string() const override;

  Root root() const override;
};

} // namespace marianatrench

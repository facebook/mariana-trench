/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Context.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/PropagationKind.h>

namespace marianatrench {

/**
 * Represents the return value of a method.
 *
 * This is used to represent a propagation within the `Taint` representation.
 * This is also used to infer propagations in the backward analysis.
 */
class LocalReturnKind final : public PropagationKind {
 public:
  explicit LocalReturnKind() = default;

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(LocalReturnKind)

  void show(std::ostream&) const override;

  std::string to_trace_string() const override;

  Root root() const override;
};

} // namespace marianatrench

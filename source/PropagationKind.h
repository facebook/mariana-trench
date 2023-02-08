/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Access.h>
#include <mariana-trench/Kind.h>

namespace marianatrench {

/**
 * Represents either the return value or argument of a method.
 * This is used in the backward analysis to infer propagations.
 */
class PropagationKind : public Kind {
 public:
  PropagationKind() = default;
  PropagationKind(const PropagationKind&) = delete;
  PropagationKind(PropagationKind&&) = delete;
  PropagationKind& operator=(const PropagationKind&) = delete;
  PropagationKind& operator=(PropagationKind&&) = delete;
  ~PropagationKind() override = default;

  virtual Root root() const = 0;
};

} // namespace marianatrench

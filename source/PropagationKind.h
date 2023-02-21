/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Access.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Kind.h>

namespace marianatrench {

/**
 * Represents either the return value or argument of a method.
 *
 * This is used to represent a propagation within the `Taint` representation.
 * In that context, the kind is the output root (parameter or return value).
 * This is also used to infer propagations in the backward analysis.
 */
class PropagationKind : public Kind {
 public:
  PropagationKind() = default;

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(PropagationKind)

  virtual Root root() const = 0;
};

} // namespace marianatrench

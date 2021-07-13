/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/GroupHashedSetAbstractDomain.h>
#include <mariana-trench/Propagation.h>

namespace marianatrench {

/**
 * A set of propagations where propagations with the same input parameter
 * position are grouped together.
 */
using PropagationSet = GroupHashedSetAbstractDomain<
    Propagation,
    Propagation::GroupHash,
    Propagation::GroupEqual>;

} // namespace marianatrench

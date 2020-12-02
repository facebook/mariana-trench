// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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

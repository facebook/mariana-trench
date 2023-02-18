/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/MemoryLocation.h>
#include <mariana-trench/PatriciaTreeSetAbstractDomain.h>

namespace marianatrench {

using MemoryLocationsDomain = PatriciaTreeSetAbstractDomain<
    MemoryLocation*,
    /* bottom_is_empty */ true,
    /* with_top */ false>;
using MemoryLocationEnvironment =
    sparta::PatriciaTreeMapAbstractPartition<Register, MemoryLocationsDomain>;

std::ostream& operator<<(
    std::ostream& out,
    const marianatrench::MemoryLocationsDomain& memory_locations);

std::ostream& operator<<(
    std::ostream& out,
    const marianatrench::MemoryLocationEnvironment& memory_locations);

} // namespace marianatrench

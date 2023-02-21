/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <boost/container/flat_map.hpp>

#include <IRInstruction.h>
#include <PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/MemoryLocation.h>
#include <mariana-trench/PatriciaTreeSetAbstractDomain.h>

namespace marianatrench {

/* A set of memory locations. */
using MemoryLocationsDomain = PatriciaTreeSetAbstractDomain<
    MemoryLocation*,
    /* bottom_is_empty */ true,
    /* with_top */ false>;

/* Mapping from registers to the memory locations they may point to,
 * with an abstract domain structure. */
using MemoryLocationEnvironment =
    sparta::PatriciaTreeMapAbstractPartition<Register, MemoryLocationsDomain>;

/* Mapping from registers to the memory locations they may point to, using
 * a concise representation. */
using RegisterMemoryLocationsMap =
    boost::container::flat_map<Register, MemoryLocationsDomain>;

RegisterMemoryLocationsMap memory_location_map_from_environment(
    const MemoryLocationEnvironment& memory_location_environment,
    const IRInstruction* instruction);

std::ostream& operator<<(
    std::ostream& out,
    const marianatrench::MemoryLocationsDomain& memory_locations);

std::ostream& operator<<(
    std::ostream& out,
    const marianatrench::MemoryLocationEnvironment& memory_locations);

} // namespace marianatrench

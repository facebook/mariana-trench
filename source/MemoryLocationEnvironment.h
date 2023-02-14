/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <PatriciaTreeMapAbstractPartition.h>
#include <PatriciaTreeSetAbstractDomain.h>

#include <mariana-trench/MemoryLocation.h>

namespace marianatrench {

using MemoryLocationsDomain =
    sparta::PatriciaTreeSetAbstractDomain<MemoryLocation*>;
using MemoryLocationEnvironment =
    sparta::PatriciaTreeMapAbstractPartition<Register, MemoryLocationsDomain>;

std::ostream& operator<<(
    std::ostream& out,
    const marianatrench::MemoryLocationEnvironment& memory_locations);

} // namespace marianatrench

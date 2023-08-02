/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <sparta/PatriciaTreeMapAbstractPartition.h>

#include <mariana-trench/MemoryLocation.h>
#include <mariana-trench/TaintTree.h>

namespace marianatrench {

using TaintEnvironment =
    sparta::PatriciaTreeMapAbstractPartition<MemoryLocation*, TaintTree>;

std::ostream& operator<<(
    std::ostream& out,
    const marianatrench::TaintEnvironment& taint);

} // namespace marianatrench

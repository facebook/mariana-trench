/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Propagation.h>
#include <mariana-trench/RootPatriciaTreeAbstractPartition.h>

namespace marianatrench {

/**
 * A partition of propagations where propagations with the same input parameter
 * position are grouped together.
 */
using PropagationPartition = RootPatriciaTreeAbstractPartition<Propagation>;

} // namespace marianatrench

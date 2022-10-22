/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/AbstractTreeDomain.h>
#include <mariana-trench/AccessPathTreeDomain.h>
#include <mariana-trench/PropagationPartition.h>

namespace marianatrench {

/**
 * An access path tree of propagations, where the path in the tree represents
 * the output path of the propagations.
 */
using PropagationAccessPathTree = AccessPathTreeDomain<PropagationPartition>;

} // namespace marianatrench

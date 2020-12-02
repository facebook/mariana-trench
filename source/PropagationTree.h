// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <mariana-trench/AbstractTreeDomain.h>
#include <mariana-trench/AccessPathTreeDomain.h>
#include <mariana-trench/PropagationSet.h>

namespace marianatrench {

using PropagationTree = AbstractTreeDomain<PropagationSet>;

/**
 * An access path tree of propagations, where the path in the tree represents
 * the output path of the propagations.
 */
using PropagationAccessPathTree = AccessPathTreeDomain<PropagationSet>;

} // namespace marianatrench

// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <mariana-trench/AbstractTreeDomain.h>
#include <mariana-trench/AccessPathTreeDomain.h>
#include <mariana-trench/Taint.h>

namespace marianatrench {

using TaintTree = AbstractTreeDomain<Taint>;
using TaintAccessPathTree = AccessPathTreeDomain<Taint>;

} // namespace marianatrench

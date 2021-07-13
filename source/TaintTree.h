/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/AbstractTreeDomain.h>
#include <mariana-trench/AccessPathTreeDomain.h>
#include <mariana-trench/Taint.h>

namespace marianatrench {

using TaintTree = AbstractTreeDomain<Taint>;
using TaintAccessPathTree = AccessPathTreeDomain<Taint>;

} // namespace marianatrench

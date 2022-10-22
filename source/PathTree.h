/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/AbstractTreeDomain.h>
#include <mariana-trench/SingletonAbstractDomain.h>

namespace marianatrench {

using PathTreeDomain = AbstractTreeDomain<SingletonAbstractDomain>;
}

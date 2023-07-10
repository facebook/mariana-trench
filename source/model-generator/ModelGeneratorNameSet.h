/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/PatriciaTreeSetAbstractDomain.h>
#include <mariana-trench/model-generator/ModelGeneratorName.h>

namespace marianatrench {

using ModelGeneratorNameSet = PatriciaTreeSetAbstractDomain<
    const ModelGeneratorName*,
    /* bottom_is_empty */ true,
    /* with_top */ false>;

} // namespace marianatrench

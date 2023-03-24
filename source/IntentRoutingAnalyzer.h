/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <mariana-trench/Method.h>
#include <mariana-trench/Types.h>

namespace marianatrench {

std::vector<const DexType*> method_routes_intents_to(
    const Method* method,
    const Types& types);

}

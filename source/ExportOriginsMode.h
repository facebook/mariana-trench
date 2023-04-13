/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

// This class is declared in its own file instead of Frame.h to get around a
// macro definition conflict between Redex and glog due to imports.

#pragma once

enum class ExportOriginsMode : unsigned int {
  // Always write origins to the output JSON. Useful for debugging.
  Always = 0,
  // SAPP only reads the leaf names on origin leaves, this conserves space
  // in the generated models.
  OnlyOnOrigins = 1,
};

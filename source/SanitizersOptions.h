/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

// This should only be included in one translation unit.

const char* kAsanDefaultOptions =
    "abort_on_error=1" // Use abort instead of exit, will get stack
                       // traces for things like ubsan.
    ":"
    "detect_leaks=0" // There are memory leaks in Redex.
    ":"
    "check_initialization_order=1"
    ":"
    "detect_invalid_pointer_pairs=1"
    ":"
    "detect_stack_use_after_return=1"
    ":"
    "strict_init_order=1";

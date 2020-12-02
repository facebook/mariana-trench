// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

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

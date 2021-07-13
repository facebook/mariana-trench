/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <Debug.h>

// Uncomment the following line to enable expensive assertions.
// #define MT_ENABLE_EXPENSIVE_ASSERT

#define mt_assert(condition) always_assert(condition)
#define mt_assert_log(condition, message, ...) \
  always_assert_log(condition, message, ##__VA_ARGS__)
#define mt_unreachable()     \
  do {                       \
    always_assert(false);    \
    __builtin_unreachable(); \
  } while (true)
#define mt_unreachable_log(message, ...)              \
  do {                                                \
    always_assert_log(false, message, ##__VA_ARGS__); \
    __builtin_unreachable();                          \
  } while (true)

#ifdef MT_ENABLE_EXPENSIVE_ASSERT
#define mt_expensive_assert(condition) always_assert(condition)
#define mt_expensive_assert_log(condition, message, ...) \
  always_assert_log(condition, message, ##__VA_ARGS__)
#define mt_if_expensive_assert(statement) statement
#else
#define mt_expensive_assert(condition) static_cast<void>(0)
#define mt_expensive_assert_log(condition, message, ...) static_cast<void>(0)
#define mt_if_expensive_assert(statement) static_cast<void>(0)
#endif

// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

// This file is mostly based on folly/CppAttributes.h

#ifndef __has_extension
#define MT_HAS_EXTENSION(x) 0
#else
#define MT_HAS_EXTENSION(x) __has_extension(x)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define MT_PUSH_WARNING _Pragma("GCC diagnostic push")
#define MT_POP_WARNING _Pragma("GCC diagnostic pop")
#define MT_GNU_DISABLE_WARNING_INTERNAL2(warningName) #warningName
#define MT_GNU_DISABLE_WARNING(warningName) \
  _Pragma(MT_GNU_DISABLE_WARNING_INTERNAL2(GCC diagnostic ignored warningName))
#ifdef __clang__
#define MT_CLANG_DISABLE_WARNING(warningName) \
  MT_GNU_DISABLE_WARNING(warningName)
#define MT_GCC_DISABLE_WARNING(warningName)
#else
#define MT_CLANG_DISABLE_WARNING(warningName)
#define MT_GCC_DISABLE_WARNING(warningName) MT_GNU_DISABLE_WARNING(warningName)
#endif
#define MT_MSVC_DISABLE_WARNING(warningNumber)
#elif defined(_MSC_VER)
#define MT_PUSH_WARNING __pragma(warning(push))
#define MT_POP_WARNING __pragma(warning(pop))
#define MT_GNU_DISABLE_WARNING(warningName)
#define MT_GCC_DISABLE_WARNING(warningName)
#define MT_CLANG_DISABLE_WARNING(warningName)
#define MT_MSVC_DISABLE_WARNING(warningNumber) \
  __pragma(warning(disable : warningNumber))
#else
#define MT_PUSH_WARNING
#define MT_POP_WARNING
#define MT_GNU_DISABLE_WARNING(warningName)
#define MT_GCC_DISABLE_WARNING(warningName)
#define MT_CLANG_DISABLE_WARNING(warningName)
#define MT_MSVC_DISABLE_WARNING(warningNumber)
#endif

/**
 * Nullable indicates that a return value or a parameter may be a `nullptr`,
 * e.g.
 *
 * int* MT_NULLABLE foo(int* a, int* MT_NULLABLE b) {
 *   if (*a > 0) {  // safe dereference
 *     return nullptr;
 *   }
 *   if (*b < 0) {  // unsafe dereference
 *     return *a;
 *   }
 *   if (b != nullptr && *b == 1) {  // safe checked dereference
 *     return new int(1);
 *   }
 *   return nullptr;
 * }
 *
 * Ignores Clang's -Wnullability-extension since it correctly handles the case
 * where the extension is not present.
 */
#if MT_HAS_EXTENSION(nullability)
#define MT_NULLABLE                                   \
  MT_PUSH_WARNING                                     \
  MT_CLANG_DISABLE_WARNING("-Wnullability-extension") \
  _Nullable MT_POP_WARNING
#else
#define MT_NULLABLE
#endif

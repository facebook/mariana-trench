/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <concepts>
#include <ostream>

#include <boost/exception/all.hpp>
#include <boost/stacktrace.hpp>

#include <mariana-trench/Compiler.h>

namespace marianatrench {

using traced =
    boost::error_info<struct tag_stacktrace, boost::stacktrace::stacktrace>;

/*
 * Creates an exception of type E with args... and attaches current stack
 * backtrace to it. The function is marked as always-inline to avoid capturing
 * the frame of itself.
 */
template <typename E, typename... Args>
  requires std::constructible_from<E, Args...> &&
    std::derived_from<E, std::exception>
MT_ALWAYS_INLINE auto exception_with_backtrace(Args&&... args) {
  // Use boost stacktrace library to get demangled symbol names
  return boost::enable_error_info(E{std::forward<Args>(args)...})
      << traced(boost::stacktrace::stacktrace());
}

/*
 * Prints the backtrace of an exception (if any) to the given stream.
 */
inline void print_exception_backtrace(
    std::ostream& os,
    const std::exception& e) {
  if (const auto* backtrace = boost::get_error_info<traced>(e)) {
    os << "Backtrace\n" << *backtrace << std::endl;
  }
}

} // namespace marianatrench

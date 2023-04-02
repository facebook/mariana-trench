/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <AbstractDomain.h>

#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/MemoryLocation.h>
#include <mariana-trench/MemoryLocationEnvironment.h>
#include <mariana-trench/MethodContext.h>
#include <mariana-trench/Taint.h>
#include <mariana-trench/TaintEnvironment.h>
#include <mariana-trench/TaintTree.h>

namespace marianatrench {

class BackwardTaintEnvironment final
    : public sparta::AbstractDomain<BackwardTaintEnvironment> {
 public:
  /* Create the bottom environment. */
  BackwardTaintEnvironment() : taint_(TaintEnvironment::bottom()) {}

  explicit BackwardTaintEnvironment(TaintEnvironment taint)
      : taint_(std::move(taint)) {}

  static BackwardTaintEnvironment initial(MethodContext& context);

  INCLUDE_ABSTRACT_DOMAIN_METHODS(
      BackwardTaintEnvironment,
      TaintEnvironment,
      taint_)

  TaintTree read(MemoryLocation* memory_location) const;

  TaintTree read(MemoryLocation* memory_location, const Path& path) const;

  TaintTree read(const MemoryLocationsDomain& memory_locations) const;

  void write(MemoryLocation* memory_location, TaintTree taint, UpdateKind kind);

  void write(
      MemoryLocation* memory_location,
      const Path& path,
      TaintTree taint,
      UpdateKind kind);

  void write(
      MemoryLocation* memory_location,
      const Path& path,
      Taint taint,
      UpdateKind kind);

  void write(
      const MemoryLocationsDomain& memory_locations,
      TaintTree taint,
      UpdateKind kind);

  void write(
      const MemoryLocationsDomain& memory_locations,
      Taint taint,
      UpdateKind kind);

  void write(
      const MemoryLocationsDomain& memory_locations,
      const Path& path,
      TaintTree taint,
      UpdateKind kind);

  void write(
      const MemoryLocationsDomain& memory_locations,
      const Path& path,
      Taint taint,
      UpdateKind kind);

  /* This must be called when accessing a specific path in backward taint. */
  static Taint propagate_output_path(Taint taint, Path::Element path_element);

  friend std::ostream& operator<<(
      std::ostream& out,
      const BackwardTaintEnvironment& environment);

 private:
  TaintEnvironment taint_;
};

} // namespace marianatrench

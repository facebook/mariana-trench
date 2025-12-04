/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <optional>
#include <string>

#include <DexPosition.h>
#include <DexStore.h>

#include <ConcurrentContainers.h>

#include <mariana-trench/Compiler.h>
#include <mariana-trench/IncludeMacros.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Position.h>

namespace marianatrench {

class LifecycleMethod;

class Positions final {
 public:
  Positions() = default;

  Positions(const Options& options, const DexStoresVector& stores);

  DELETE_COPY_CONSTRUCTORS_AND_ASSIGNMENTS(Positions)

  /**
   * If the `position` parameter is a `nullptr` we will try to find the first
   * position in the given method.
   */
  const Position* get(
      const DexMethod* method,
      const DexPosition* MT_NULLABLE position = nullptr,
      std::optional<Root> port = {},
      const IRInstruction* MT_NULLABLE instruction = nullptr) const;

  const Position* get(
      const Method* method,
      const DexPosition* MT_NULLABLE position = nullptr,
      std::optional<Root> port = {},
      const IRInstruction* MT_NULLABLE instruction = nullptr) const;

  const Position* get(
      const DexMethod* method,
      int line,
      std::optional<Root> port = {},
      const IRInstruction* MT_NULLABLE instruction = nullptr,
      int start = k_unknown_start,
      int end = k_unknown_end) const;

  const Position* get(
      const std::optional<std::string>& path,
      int line,
      std::optional<Root> port = {},
      const IRInstruction* MT_NULLABLE instruction = nullptr,
      int start = k_unknown_start,
      int end = k_unknown_end) const;

  const Position*
  get(const std::string* MT_NULLABLE path, int line, int start, int end) const;

  const Position* get(
      const Position* position,
      std::optional<Root> port,
      const IRInstruction* instruction) const;

  const Position* get(const Position* position, int line, int start, int end)
      const;

  const Position* unknown() const;

  const std::string* MT_NULLABLE get_path(const DexMethod* method) const;

  void set_lifecycle_wrapper_path(const LifecycleMethod& method);

  std::unordered_set<const std::string*> all_paths() const;

  static std::string execute_and_catch_output(
      const std::string& command,
      int& return_code);

 private:
  mutable InsertOnlyConcurrentSet<std::string> paths_;
  mutable InsertOnlyConcurrentSet<Position> positions_;
  ConcurrentMap<const DexMethod*, const std::string*> method_to_path_;
  ConcurrentMap<const DexMethod*, int> method_to_line_;
};

} // namespace marianatrench

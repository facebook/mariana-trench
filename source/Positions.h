/*
 * Copyright (c) Facebook, Inc. and its affiliates.
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

#include <mariana-trench/Method.h>
#include <mariana-trench/Options.h>
#include <mariana-trench/Position.h>

namespace marianatrench {

class Positions final {
 public:
  Positions();
  Positions(const Options& options, const DexStoresVector& stores);

  Positions(const Positions&) = delete;
  Positions(Positions&&) = delete;
  Positions& operator=(const Positions&) = delete;
  Positions& operator=(Positions&&) = delete;
  ~Positions() = default;

  /**
   * If the `position` parameter is a `nullptr` we will try to find the first
   * position in the given method.
   */
  const Position* get(
      const DexMethod* method,
      const DexPosition* position = nullptr,
      std::optional<Root> port = {},
      const IRInstruction* instruction = nullptr) const;

  const Position* get(
      const Method* method,
      const DexPosition* position = nullptr,
      std::optional<Root> port = {},
      const IRInstruction* instruction = nullptr) const;

  const Position* get(
      const DexMethod* method,
      int line,
      std::optional<Root> port = {},
      const IRInstruction* instruction = nullptr) const;

  const Position* get(
      const std::optional<std::string>& path,
      int line,
      std::optional<Root> port = {},
      const IRInstruction* instruction = nullptr) const;

  const Position* get(
      const Position* position,
      std::optional<Root> port,
      const IRInstruction* instruction) const;

  const Position* unknown() const;

 private:
  mutable InsertOnlyConcurrentSet<std::string> paths_;
  mutable InsertOnlyConcurrentSet<Position> positions_;
  ConcurrentMap<const DexMethod*, const std::string*> method_to_path_;
  ConcurrentMap<const DexMethod*, int> method_to_line_;
};

} // namespace marianatrench

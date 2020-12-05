/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <boost/container/flat_map.hpp>

#include <DexClass.h>
#include <TypeInference.h>

#include <mariana-trench/Access.h>
#include <mariana-trench/Compiler.h>
#include <mariana-trench/Method.h>
#include <mariana-trench/UniquePointerConcurrentMap.h>

namespace marianatrench {

using TypeEnvironment = boost::container::flat_map<Register, const DexType*>;

using TypeEnvironments =
    std::unordered_map<const IRInstruction*, TypeEnvironment>;

class Types final {
 public:
  Types() = default;
  explicit Types(const DexStoresVector& stores);
  Types(const Types&) = delete;
  Types(Types&&) = delete;
  Types& operator=(const Types&) = delete;
  Types& operator=(Types&&) = delete;
  ~Types() = default;

  const TypeEnvironments& environments(const Method* method) const;

  const TypeEnvironment& environment(
      const Method* method,
      const IRInstruction* instruction) const;

  /**
   * Get the type of a register at the given instruction.
   *
   * Returns `nullptr` if we could not infer the type.
   */
  const DexType* MT_NULLABLE register_type(
      const Method* method,
      const IRInstruction* instruction,
      Register register_id) const;

  /**
   * Get the type of the given source of the given instruction.
   *
   * Returns `nullptr` if we could not infer the type.
   */
  const DexType* MT_NULLABLE source_type(
      const Method* method,
      const IRInstruction* instruction,
      std::size_t source_position) const;

  /**
   * Get the receiver type of an invoke instruction.
   *
   * Returns `nullptr` if we could not infer the type.
   */
  const DexType* MT_NULLABLE
  receiver_type(const Method* method, const IRInstruction* instruction) const;

 private:
  mutable UniquePointerConcurrentMap<const Method*, TypeEnvironments>
      environments_;
};

} // namespace marianatrench

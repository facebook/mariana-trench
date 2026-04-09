/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <variant>

#include <json/json.h>

namespace marianatrench {
namespace local_flow {

/**
 * Node kinds in the local flow graph.
 *
 * Per-callable roots: Param, Result, This, ThisOut, Ctrl
 * Global/stable nodes (survive temp elimination): Global, Meth, CVar, OVar,
 *   SelfRec, Constant
 * Local temporaries (eliminated during closure): Temp
 */
enum class NodeKind {
  // Per-callable roots
  Param,
  Result,
  This,
  ThisOut,
  Ctrl,

  // Global/stable nodes
  Global,
  Meth,
  CVar,
  OVar,
  SelfRec,
  Constant,

  // Local temporaries
  Temp,
};

/**
 * A node in the local flow graph.
 *
 * Different node kinds carry different payloads:
 * - Param: parameter index (uint32_t)
 * - Result, This, ThisOut, Ctrl, SelfRec: no payload
 * - Global, Meth, CVar, OVar: qualified name (string)
 * - Constant: constant value representation (string)
 * - Temp: unique temporary id (uint64_t)
 */
class LocalFlowNode {
 public:
  // Factory methods for creating nodes
  static LocalFlowNode make_param(uint32_t index);
  static LocalFlowNode make_result();
  static LocalFlowNode make_this();
  static LocalFlowNode make_this_out();
  static LocalFlowNode make_ctrl();
  static LocalFlowNode make_global(std::string name);
  static LocalFlowNode make_meth(std::string name);
  static LocalFlowNode make_cvar(std::string name);
  static LocalFlowNode make_ovar(std::string name);
  static LocalFlowNode make_self_rec();
  static LocalFlowNode make_constant(std::string value);
  static LocalFlowNode make_temp(uint64_t id);

  NodeKind kind() const;

  /**
   * Returns true if this is a temporary node (eliminated during closure).
   */
  bool is_temp() const;

  /**
   * Returns true if this is a per-callable root node (Param, Result, This,
   * ThisOut, Ctrl).
   */
  bool is_root() const;

  /**
   * Returns true if this is a global/stable node that survives temp
   * elimination.
   */
  bool is_global() const;

  /**
   * Returns true if this is a callee node (Global or Meth) — used to
   * determine whether to use Project vs ContraProject for source labels
   * in self-loop serialization.
   */
  bool is_callee_node() const;

  bool operator==(const LocalFlowNode& other) const;
  bool operator!=(const LocalFlowNode& other) const;
  bool operator<(const LocalFlowNode& other) const;

  std::string to_string() const;
  Json::Value to_json() const;

  /**
   * Convert JVM method/field signature separator from "." to "||".
   * "Lcom/facebook/Foo;.bar:(I)V" → "Lcom/facebook/Foo;||bar:(I)V"
   * Class-only names without ";." are returned unchanged.
   */
  static std::string jvm_to_lfe_separator(const std::string& jvm_sig);

  /**
   * Extract vtable field name from a Global/Meth node for LFE self-loop labels.
   * Returns everything after "||" (including full JVM signature for overload
   * disambiguation). For non-method nodes, returns to_string().
   */
  static std::string extract_vtable_name(const LocalFlowNode& node);

  /**
   * Extract class name from a Meth node for dispatch table routing.
   * Returns everything before "||".
   * E.g., "Lcom/Foo;||bar:(I)V" → "Lcom/Foo;"
   */
  static std::string extract_class_name(const LocalFlowNode& node);

 private:
  NodeKind kind_;

  // Payload varies by kind
  struct NoPayload {};
  struct ParamPayload {
    uint32_t index;
  };
  struct NamePayload {
    std::string name;
  };
  struct TempPayload {
    uint64_t id;
  };

  using Payload =
      std::variant<NoPayload, ParamPayload, NamePayload, TempPayload>;
  Payload payload_;

  LocalFlowNode(NodeKind kind, Payload payload);
};

/**
 * Thread-safe counter for generating unique temporary node IDs.
 */
class TempIdGenerator {
 public:
  TempIdGenerator();
  uint64_t next();

 private:
  std::atomic<uint64_t> next_id_;
};

} // namespace local_flow
} // namespace marianatrench

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/local_flow/LocalFlowMethodAnalyzer.h>
#include <mariana-trench/local_flow/LocalFlowTempElimination.h>
#include <mariana-trench/local_flow/LocalFlowTransfer.h>

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>

#include <ControlFlow.h>
#include <DexPosition.h>
#include <GraphUtil.h>
#include <IRCode.h>
#include <IRList.h>

#include <mariana-trench/Log.h>
#include <mariana-trench/Positions.h>

namespace marianatrench {
namespace local_flow {

namespace {

RegisterEnvironment join_envs(
    const std::vector<RegisterEnvironment*>& pred_envs,
    TempIdGenerator& temp_gen,
    LocalFlowConstraintSet& constraints) {
  if (pred_envs.size() == 1) {
    return *pred_envs[0];
  }

  // Collect all registers present in any predecessor
  std::set<reg_t> all_regs;
  for (const auto* env : pred_envs) {
    for (const auto& [reg, _] : *env) {
      all_regs.insert(reg);
    }
  }

  RegisterEnvironment result;
  for (auto reg : all_regs) {
    // Collect trees from predecessors that have this register
    std::vector<const LocalFlowValueTree*> trees;
    for (const auto* env : pred_envs) {
      auto it = env->find(reg);
      if (it != env->end()) {
        trees.push_back(&it->second);
      }
    }

    if (trees.empty()) {
      continue;
    }

    if (trees.size() == 1) {
      result.emplace(reg, *trees[0]);
      continue;
    }

    // Flat N-way join: single temp with direct fan-in from all predecessors.
    // Avoids O(N²) linear temp chain from pairwise join.
    result.emplace(
        reg, LocalFlowValueTree::join_all(trees, temp_gen, constraints));
  }

  return result;
}

/**
 * Scan blocks reachable from a loop head to find which registers are assigned
 * within the loop body. A block is in the loop body if it has higher RPO index
 * than the loop head and has a path back to the loop head via back edges.
 *
 * For simplicity, we collect all registers assigned in blocks that are between
 * the loop head and any block with a back edge to the loop head.
 */
std::set<reg_t> find_assigned_registers(
    const std::vector<cfg::Block*>& rpo_blocks,
    const std::unordered_map<cfg::Block*, std::size_t>& rpo_index,
    cfg::Block* loop_head,
    const std::set<cfg::Block*>& back_edge_sources) {
  std::set<reg_t> assigned;
  auto head_idx = rpo_index.at(loop_head);

  // Find the max RPO index among back-edge sources to bound the loop body
  std::size_t max_body_idx = head_idx;
  for (auto* src : back_edge_sources) {
    auto it = rpo_index.find(src);
    if (it != rpo_index.end()) {
      max_body_idx = std::max(max_body_idx, it->second);
    }
  }

  // Scan all blocks in the loop body range [head_idx, max_body_idx]
  for (std::size_t i = head_idx; i <= max_body_idx && i < rpo_blocks.size();
       i++) {
    auto* block = rpo_blocks[i];
    for (auto it = block->begin(); it != block->end(); ++it) {
      if (it->type != MFLOW_OPCODE) {
        continue;
      }
      auto* insn = it->insn;
      if (insn->has_dest()) {
        assigned.insert(insn->dest());
      }
      // INVOKE/SGET/IGET results go to the following MOVE_RESULT_PSEUDO
      // which has a dest, so they're covered above.
    }
  }

  return assigned;
}

} // namespace

std::optional<LocalFlowMethodResult> LocalFlowMethodAnalyzer::analyze(
    const Method* method,
    std::size_t max_structural_depth,
    const Positions* MT_NULLABLE positions,
    const CallGraph* MT_NULLABLE call_graph) {
  auto* dex_method = method->dex_method();
  if (dex_method == nullptr) {
    return std::nullopt;
  }

  auto* code = dex_method->get_code();
  if (code == nullptr) {
    return std::nullopt;
  }

  // External classes (from system JARs) may have IRCode but no CFG built,
  // since ControlFlowGraphs filters out is_external() classes.
  if (!code->cfg_built()) {
    return std::nullopt;
  }

  auto& cfg = code->cfg();

  TempIdGenerator temp_gen;
  LocalFlowTransfer transfer(method, temp_gen, positions, call_graph);
  LocalFlowConstraintSet constraints;

  // Compute reverse-post-order for block traversal
  auto rpo_blocks = graph::postorder_sort<cfg::GraphInterface>(cfg);
  std::reverse(rpo_blocks.begin(), rpo_blocks.end());

  auto* entry_block = cfg.entry_block();

  // Build RPO index map for back-edge detection
  std::unordered_map<cfg::Block*, std::size_t> rpo_index;
  for (std::size_t i = 0; i < rpo_blocks.size(); i++) {
    rpo_index[rpo_blocks[i]] = i;
  }

  // Detect back edges and identify loop heads.
  // An edge (A, B) is a back edge if rpo_index[B] <= rpo_index[A].
  std::set<cfg::Block*> loop_heads;
  // Map from loop head -> set of back-edge source blocks
  std::map<cfg::Block*, std::set<cfg::Block*>> loop_head_sources;

  for (auto* block : rpo_blocks) {
    for (auto* edge : block->succs()) {
      if (edge->type() == cfg::EDGE_THROW) {
        continue;
      }
      auto* target = edge->target();
      auto target_it = rpo_index.find(target);
      auto source_it = rpo_index.find(block);
      if (target_it != rpo_index.end() && source_it != rpo_index.end() &&
          target_it->second <= source_it->second) {
        loop_heads.insert(target);
        loop_head_sources[target].insert(block);
      }
    }
  }

  // For each loop head, pre-compute which registers are assigned in the body
  // and create loop-head temp nodes for them.
  // loop_head_temps[block][reg] = temp node for that register at loop head
  std::map<cfg::Block*, std::map<reg_t, LocalFlowNode>> loop_head_temps;
  for (auto* head : loop_heads) {
    auto assigned = find_assigned_registers(
        rpo_blocks, rpo_index, head, loop_head_sources[head]);
    std::map<reg_t, LocalFlowNode> temps;
    for (auto reg : assigned) {
      temps.emplace(reg, LocalFlowNode::make_temp(temp_gen.next()));
    }
    loop_head_temps[head] = std::move(temps);
  }

  // Per-block exit states
  std::map<cfg::Block*, RegisterEnvironment> exit_states;

  for (auto* block : rpo_blocks) {
    RegisterEnvironment entry_env;

    if (block == entry_block) {
      // Entry block: start with empty env (LOAD_PARAM will populate)
    } else {
      // Collect non-throw, non-back-edge predecessor exit states
      std::vector<RegisterEnvironment*> pred_envs;
      for (auto* edge : block->preds()) {
        if (edge->type() == cfg::EDGE_THROW) {
          continue;
        }
        auto* pred = edge->src();
        // Skip back edges (pred with higher or equal RPO index targeting us)
        auto pred_it = rpo_index.find(pred);
        auto block_it = rpo_index.find(block);
        if (pred_it != rpo_index.end() && block_it != rpo_index.end() &&
            pred_it->second >= block_it->second) {
          continue; // Back edge — skip
        }
        auto it = exit_states.find(pred);
        if (it != exit_states.end()) {
          pred_envs.push_back(&it->second);
        }
      }

      if (pred_envs.empty()) {
        // Dead block (no reachable predecessors) — skip
        continue;
      }

      entry_env = join_envs(pred_envs, temp_gen, constraints);
    }

    // If this is a loop head, replace assigned registers with loop-head temps.
    // Emit constraints from the entry state to the loop-head temps (so the
    // non-loop path's value flows through).
    if (loop_heads.count(block) > 0) {
      const auto& temps = loop_head_temps[block];
      for (const auto& [reg, temp_node] : temps) {
        auto it = entry_env.find(reg);
        if (it != entry_env.end()) {
          // Entry value -> loop-head temp (for 0 iteration case)
          constraints.add_edge(it->second.root(), temp_node);
        }
        // Replace register with loop-head temp
        entry_env.insert_or_assign(reg, LocalFlowValueTree(temp_node));
      }
    }

    // Walk instructions in this block, tracking DexPosition for call-site
    // annotation on Call/Return labels.
    const DexPosition* current_position = nullptr;
    for (auto it = block->begin(); it != block->end(); ++it) {
      if (it->type == MFLOW_POSITION) {
        current_position = it->pos.get();
        continue;
      }
      if (it->type != MFLOW_OPCODE) {
        continue;
      }
      transfer.analyze_instruction(
          it->insn, entry_env, constraints, current_position);
    }

    exit_states.emplace(block, std::move(entry_env));
  }

  // After processing all blocks, connect back-edge source exit states
  // to loop-head temps. This models the loop body's effect on registers.
  for (auto* head : loop_heads) {
    const auto& temps = loop_head_temps[head];
    for (auto* back_src : loop_head_sources[head]) {
      auto src_it = exit_states.find(back_src);
      if (src_it == exit_states.end()) {
        continue;
      }
      const auto& src_env = src_it->second;
      for (const auto& [reg, temp_node] : temps) {
        auto reg_it = src_env.find(reg);
        if (reg_it != src_env.end()) {
          // Back-edge value -> loop-head temp
          constraints.add_edge(reg_it->second.root(), temp_node);
        }
      }
    }
  }

  // For instance methods, emit this -> this_out at method exit.
  // Find the exit block's final state for the this register and connect it
  // to ThisOut. This captures mutations to `this` (setters, builders, etc.).
  bool is_static = method->get_access() & ACC_STATIC;
  if (!is_static) {
    auto this_reg = transfer.this_register();
    if (this_reg.has_value()) {
      for (auto* block : rpo_blocks) {
        auto state_it = exit_states.find(block);
        if (state_it == exit_states.end()) {
          continue;
        }
        // Check if this block ends with a return
        bool has_return = false;
        for (auto it = block->begin(); it != block->end(); ++it) {
          if (it->type != MFLOW_OPCODE) {
            continue;
          }
          auto op = it->insn->opcode();
          if (op == OPCODE_RETURN_VOID || op == OPCODE_RETURN ||
              op == OPCODE_RETURN_OBJECT || op == OPCODE_RETURN_WIDE) {
            has_return = true;
          }
        }
        if (!has_return) {
          continue;
        }
        const auto& exit_env = state_it->second;
        auto reg_it = exit_env.find(*this_reg);
        if (reg_it != exit_env.end()) {
          constraints.add_edge(
              reg_it->second.root(), LocalFlowNode::make_this_out());
        }
      }
    }
  }

  // Run temp elimination (bounded closure)
  if (!LocalFlowTempElimination::eliminate(constraints, max_structural_depth)) {
    WARNING(
        1,
        "Temp elimination exceeded composed-edge limit, skipping method `{}`.",
        method->signature());
    return std::nullopt;
  }

  // Populate path and callable_line from Positions if available
  std::string result_path;
  int result_callable_line = 0;
  if (positions != nullptr) {
    const auto* path_ptr = positions->get_path(dex_method);
    if (path_ptr != nullptr) {
      result_path = *path_ptr;
    }
    const auto* pos = positions->get(dex_method);
    if (pos != nullptr && pos->line() >= 0) {
      result_callable_line = pos->line();
    }
  }

  return LocalFlowMethodResult{
      LocalFlowNode::jvm_to_lfe_separator(method->signature()),
      std::move(constraints),
      transfer.relevant_types(),
      std::move(result_path),
      result_callable_line,
      is_static};
}

} // namespace local_flow
} // namespace marianatrench

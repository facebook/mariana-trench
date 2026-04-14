/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mariana-trench/local_flow/LocalFlowClassAnalyzer.h>

#include <string_view>

#include <ConcurrentContainers.h>
#include <DexAccess.h>
#include <DexClass.h>
#include <DexStore.h>
#include <Show.h>
#include <TypeUtil.h>
#include <Walkers.h>

#include <sparta/WorkQueue.h>

#include <fmt/format.h>

#include <mariana-trench/ClassIntervals.h>
#include <mariana-trench/Log.h>

namespace marianatrench {
namespace local_flow {

namespace {

std::string format_interval(const ClassIntervals::Interval& interval) {
  if (interval.is_bottom()) {
    return "Interval:[]";
  }
  return fmt::format(
      "Interval:[{},{}]", interval.lower_bound(), interval.upper_bound());
}

} // namespace

std::vector<LocalFlowClassResult> LocalFlowClassAnalyzer::analyze_classes(
    const Context& context,
    const std::unordered_set<const DexType*>* relevant_types) {
  ConcurrentMap<const DexType*, LocalFlowClassResult> results_map;
  ConcurrentMap<const DexType*, std::vector<DispatchEdge>>
      parent_override_edges;

  const auto* class_intervals = context.class_intervals.get();

  auto extract_metadata =
      [&](const DexClass* klass,
          const DexType* class_type) -> std::pair<std::string, std::string> {
    std::string class_path;
    auto* source_file = klass->get_source_file();
    if (source_file != nullptr) {
      class_path = source_file->str();
    }

    std::string interval_str;
    if (class_intervals != nullptr) {
      const auto& class_interval = class_intervals->get_interval(class_type);
      if (!class_interval.is_bottom()) {
        interval_str = fmt::format(
            "[{},{}]",
            class_interval.lower_bound(),
            class_interval.upper_bound());
      }
    }

    return {std::move(class_path), std::move(interval_str)};
  };

  // Phase 1: Parallel walk — generate self edges (M→C, C→O) and collect
  // override hierarchy edges (O{Self}→O{Parent}) for parent entries.
  for (const auto& scope : DexStoreClassesIterator(context.stores)) {
    walk::parallel::classes(scope, [&](const DexClass* klass) {
      // Demand-driven: skip classes not referenced during method analysis
      if (relevant_types != nullptr &&
          relevant_types->count(klass->get_type()) == 0) {
        return;
      }

      auto* class_type = klass->get_type();
      std::string class_name = show(class_type);

      if (is_interface(klass)) {
        return;
      }

      // Collect concrete instance methods (virtual + non-static direct)
      std::vector<const DexMethod*> instance_methods;
      for (const auto* vmethod : klass->get_vmethods()) {
        if (!is_abstract(vmethod)) {
          instance_methods.push_back(vmethod);
        }
      }
      for (const auto* dmethod : klass->get_dmethods()) {
        if (!is_static(dmethod) &&
            std::string_view(dmethod->get_name()->c_str()) != "<init>" &&
            std::string_view(dmethod->get_name()->c_str()) != "<clinit>") {
          instance_methods.push_back(dmethod);
        }
      }

      if (instance_methods.empty()) {
        return;
      }

      auto cvar_node = LocalFlowNode::make_cvar(class_name);
      auto ovar_node = LocalFlowNode::make_ovar(class_name);

      // 1. Meth -> CVar edges for each declared method
      // 2. CVar -> OVar edge
      std::vector<DispatchEdge> self_edges;
      {
        for (const auto* method : instance_methods) {
          std::string method_sig =
              LocalFlowNode::jvm_to_lfe_separator(show(method));
          auto meth_node = LocalFlowNode::make_meth(method_sig);

          auto vtable_name = LocalFlowNode::extract_vtable_name(meth_node);
          self_edges.push_back(
              DispatchEdge{
                  std::move(meth_node),
                  StructuralLabelPath{
                      StructuralLabel{StructuralOp::Inject, vtable_name}},
                  cvar_node});
        }

        self_edges.push_back(
            DispatchEdge{cvar_node, StructuralLabelPath{}, ovar_node});
      }

      std::string interval_label;
      if (class_intervals != nullptr) {
        const auto& interval = class_intervals->get_interval(class_type);
        interval_label = format_interval(interval);
      } else {
        interval_label = "Interval:[]";
      }

      auto emit_override_to_parent = [&](const DexType* parent_type) {
        DispatchEdge edge{
            ovar_node,
            StructuralLabelPath{
                StructuralLabel{StructuralOp::Inject, interval_label}},
            LocalFlowNode::make_ovar(show(parent_type))};
        parent_override_edges.update(
            parent_type,
            [&](const DexType*, std::vector<DispatchEdge>& edges, bool) {
              edges.push_back(std::move(edge));
            });
      };

      // 3. Supertype override edge
      auto* super_type = klass->get_super_class();
      if (super_type != nullptr && super_type != type::java_lang_Object()) {
        emit_override_to_parent(super_type);
      }

      // 4. Interface override edges
      auto* interfaces = klass->get_interfaces();
      if (interfaces != nullptr) {
        for (auto* intf_type : *interfaces) {
          emit_override_to_parent(intf_type);
        }
      }

      auto [class_path, interval_str] = extract_metadata(klass, class_type);

      results_map.emplace(
          class_type,
          LocalFlowClassResult{
              std::move(class_name),
              std::move(self_edges),
              std::move(class_path),
              /* callable_line */ 0,
              std::move(interval_str)});
    });
  }

  // Phase 2: Merge override edges into parent entries.
  for (auto& [parent_type, edges] : UnorderedIterable(parent_override_edges)) {
    auto it = results_map.find(parent_type);
    if (it != results_map.end()) {
      for (auto& edge : edges) {
        it->second.dispatch_edges.push_back(std::move(edge));
      }
    } else {
      // Parent not in results — create a stub entry with override edges.
      std::string parent_name = show(parent_type);
      std::string parent_path;
      std::string parent_interval;
      const auto* parent_klass = type_class(parent_type);
      if (parent_klass != nullptr) {
        std::tie(parent_path, parent_interval) =
            extract_metadata(parent_klass, parent_type);
      }
      results_map.emplace(
          parent_type,
          LocalFlowClassResult{
              std::move(parent_name),
              std::move(edges),
              std::move(parent_path),
              /* callable_line */ 0,
              std::move(parent_interval)});
    }
  }

  std::vector<LocalFlowClassResult> results;
  for (auto& [type, result] : UnorderedIterable(results_map)) {
    results.push_back(std::move(result));
  }

  LOG(1,
      "LocalFlowClassAnalyzer: generated dispatch edges for {} classes.",
      results.size());

  return results;
}

} // namespace local_flow
} // namespace marianatrench
